/*
 * CS3600, Spring 2014
 * Class Project
 * Conor Golden & Arjun Rao
 * (c) 2013 Alan Mislove
 *
 */
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "3600sendrecv.h"

static int DATA_SIZE = 1460;
unsigned short p_created = 1; // number of packets created + 1
//Used to keep track of what is being sent
struct timeval start_time; // start time
struct timeval current_time; // current time
unsigned int elapsed_time = 0;
unsigned int timeout_sec = SENDER_TIMEOUT_SEC; // seconds, gets modified
unsigned int timeout_usec = SENDER_TIMEOUT_MICRO; // microseconds, gets modified

void usage() {
    printf("Usage: 3600send host:port\n");
    exit(1);
}

/**
 * Reads the next block of data from stdin
 */
int get_next_data(char *data, int size) {
    return read(0, data, size);
}

/**
 * Builds and returns the next packet, or NULL
 * if no more data is available.
 */
void *get_next_packet(int sequence, int *len, unsigned int time) {
    //Because packets may need retransmission, store sent packets in a buffer
    char *data = malloc(DATA_SIZE);
    int data_len = get_next_data(data, DATA_SIZE); // get data and data length

    if (data_len == 0) { // if we got no data return NULL
        free(data);
        return NULL;
    }

    // create a header with an appropriate sequence number, length and time
    header *myheader = make_header((short)sequence, data_len, 0, 0, time);
    void *packet = malloc(sizeof(header) + sizeof(char) + data_len);
    memcpy(packet, myheader, sizeof(header));
    
    // create a checksum and append it after the header
    unsigned char *checksum = malloc(sizeof(char));
    *checksum = get_checksum((char *)myheader, data, data_len);
    memcpy(((char *) packet) +sizeof(header), (char *)checksum, sizeof(unsigned char));
   
    // append the data to the packet
    memcpy(((char *) packet) +sizeof(header)+sizeof(char), data, data_len);

    // free allocated things that aren't the packet
    free(data);
    free(checksum);
    free(myheader);

    // measure the length
    *len = sizeof(header) + sizeof(char) + data_len;

    // return completed packet
    return packet;
}


/**
 * Builds and returns the final EOF packet.
 */
void *get_final_packet() {
    // get the current time
    gettimeofday(&current_time, NULL);

    // get the elapsed time since the program started creating packets
    elapsed_time = (unsigned int)(current_time.tv_sec*1000000 + current_time.tv_usec)-(start_time.tv_sec*100000+start_time.tv_usec);

    // make a header with the EOF flag set
    header *myheader = make_header(p_created, 0, 1, 0, elapsed_time);
    void *packet = malloc(sizeof(header) + sizeof(char));
    memcpy(packet, myheader, sizeof(header));

    // create a checksum for that header
    unsigned char *checksum = malloc(sizeof(char));
    *checksum = get_checksum((char *)myheader, NULL, 0);

    // combine the two into a packet
    memcpy(((char *) packet) +sizeof(header), (char *)checksum, sizeof(unsigned char));

    // free everything besides the packet
    free(checksum);
    free(myheader);

    // return completed packet
    return packet;
}

/**
 * Gets the RTT of the data and the ack and changes the average of these timeouts. It is void
 * and only changes the timeouts.
 */
void timeout_adjust(unsigned int time) {
    // convert the time from network time
    time = ntohl(time);
    // get the current time
    gettimeofday(&current_time, NULL);
    // calculate the elapsed time since the program started creating packets in microseconds
    elapsed_time = (unsigned int)(current_time.tv_sec*1000000 + current_time.tv_usec)-(start_time.tv_sec*100000+start_time.tv_usec);
    unsigned int rtt = elapsed_time - time; // calculate round-trip time
    // update second and microsecond timeouts accordingly
    timeout_sec = timeout_sec*RTT_DECAY + (1-RTT_DECAY)*(rtt)/1000000;
    timeout_usec =  timeout_usec*RTT_DECAY + ((unsigned int)((1.0-RTT_DECAY)*(rtt)))%1000000;
}

int main(int argc, char *argv[]) {
    // extract the host IP and port
    if ((argc != 2) || (strstr(argv[1], ":") == NULL)) {
        usage();
    }

    char *tmp = (char *) malloc(strlen(argv[1])+1);
    strcpy(tmp, argv[1]);

    char *ip_s = strtok(tmp, ":");
    char *port_s = strtok(NULL, ":");

    // first, open a UDP socket  
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // next, construct the local port
    struct sockaddr_in out;
    out.sin_family = AF_INET;
    out.sin_port = htons(atoi(port_s));
    out.sin_addr.s_addr = inet_addr(ip_s);

    // socket for received packets
    struct sockaddr_in in;
    socklen_t in_len = sizeof(in);

    // construct the socket set
    fd_set socks;

    // construct the timeout
    struct timeval t;
    t.tv_sec = SENDER_TIMEOUT_SEC;
    t.tv_usec = SENDER_TIMEOUT_MICRO; // 10 ms

    // packet tracking vars
    unsigned short p_ack = 0; // acknowledged packets
    void * packets[WINDOW_SIZE] = {0}; // array of pointers to packets for sending
    int p_len[WINDOW_SIZE] = {0}; // 
    int more_packets = 1; // whether there is more data to create packets for
    int i = 0; // counting variable
    int repeated_acks = 0; // # of repeated ACKs received in current round
    int retransmitted = 0; // whether we are or just have fast retransmit
    int starting = 1; // if we are in the starting phase scaling the window more aggressively
    int sliding_window = 2000; // the starting window size to use
    int backoff = 1; // how much the window will be downscaled on backoff
    int round_acked = 0; // the number of ACKs received in the current round

    // allocate memory buffers for packets
    for (i = 0; i < WINDOW_SIZE; i++) {
        packets[i] = calloc(1, 1500);
    }
    
    // get the starting and current time
    gettimeofday(&start_time, NULL); //method built into C
    gettimeofday(&current_time, NULL);

    while (1) {
        // get the next packets to send if necessary in window
        while (more_packets && p_created-p_ack <= sliding_window) {
            // get a send time for our new packet
            gettimeofday(&current_time, NULL);
            elapsed_time = (unsigned int)(current_time.tv_sec*1000000 + current_time.tv_usec)-(start_time.tv_sec*100000+start_time.tv_usec);
            // create the packet and add it to the array
            packets[p_created%WINDOW_SIZE] = get_next_packet(p_created, &(p_len[p_created%WINDOW_SIZE]), elapsed_time); // 
             
            if (packets[p_created%WINDOW_SIZE] == NULL) { // if no more data needs packets
                more_packets = 0;
            }
            else { // otherwise increase our packet created count
                p_created++;
            }
        }

        // send non-acked packets in window
        for (i = p_ack+1; i < p_created && i <= p_ack+sliding_window; i++) { //one is added to pass the huge 5mb test
            // for all of the packets we created in the current window
            if (sendto(sock, packets[i%WINDOW_SIZE], p_len[i%WINDOW_SIZE], 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) { // send them and ensure that there were no probs.
                perror("sendto");
                exit(1);
            }
            // log sent data
            mylog("[send data] %d (%d)\n", i, p_len[i%WINDOW_SIZE] - sizeof(header));
            if (retransmitted) { // if we were fast retransmitting
                // send it twice in case of another drop
                if (sendto(sock, packets[i%WINDOW_SIZE], p_len[i%WINDOW_SIZE], 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
                    perror("sendto");
                    exit(1);
                }
                if (sendto(sock, packets[i%WINDOW_SIZE], p_len[i%WINDOW_SIZE], 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
                    perror("sendto");
                    exit(1);
                }
                break; // break because we don't need to send the rest
            }
        }
        
        // set sockets and timeouts for receiving ACKs
        FD_ZERO(&socks);
        FD_SET(sock, &socks);
        t.tv_sec = timeout_sec * RTT_MULT;
        t.tv_usec = timeout_usec * RTT_MULT;

        // wait to receive, or for a timeout
        while (select(sock + 1, &socks, NULL, NULL, &t)) {
            unsigned char buf[10000]; //buffer of data
            int buf_len = sizeof(buf);
            int received;
            if ((received = recvfrom(sock, &buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
                perror("recvfrom");
                exit(1);
            }
            
            // get the checksum and header from the received packet
            unsigned char expected_checksum = get_checksum((char *)buf, NULL, 0); 
            header *myheader = get_header(buf);

            if (retransmitted && myheader->sequence == p_ack) {                
                timeout_adjust(myheader->time); // update timeouts
                continue; // go to process next packet and ignore this one
            }

            if ((myheader->magic == MAGIC) && expected_checksum == *((unsigned char *)(buf + sizeof(header))) && (myheader->sequence >= p_ack) && (myheader->ack == 1)) {
                // checksum checks out
                mylog("[recv ack] %d\n", myheader->sequence); // log the receive

                if (p_ack == myheader->sequence) { // if we received a repeated ack
                    repeated_acks++; // count it
                }
                else { // if we are in slow start, and the ack was successful
                    if (starting) {
                        sliding_window++; // increased window size by 1 for every ACK
                    }
                    else {
                        // not in slow start but in congestion avoidance
                        round_acked++;
                    }
                    retransmitted = 0; // reset potential for a fast_retransmit
                    repeated_acks = 0; // reset repeated ack count
                }

                if (repeated_acks >= DUPLICATE_ACKS) { // recieved duplicate acks
                    retransmitted = 1; // needs fast retransmit
                    repeated_acks = 0; // reset counts
                    starting = 0; // exit slow start
                    backoff++; // increment the backoff by one
                    timeout_adjust(myheader->time); // increase the timeout
                    break; // exit for fast retransmit
                }

                p_ack = myheader->sequence; // set new sequence #
                timeout_adjust(myheader->time); // update timeout values estimates based on RTT
                if (p_ack+1 == p_created) { // if we got back all the packets we created
                    break; // we don't need to wait for the timeout so break
                }
            } else { // error
                mylog("[recv ack error] %x %d\n", MAGIC, p_created);
            }

        } 
        // increase the window size
        sliding_window += (int) (CWD_SCALE_FACTOR*(float)round_acked/(float)sliding_window);
        round_acked = 0;

        if (t.tv_sec <= 0 && t.tv_usec <= 0) { // if we timed out waiting for packets
            starting = 0; // slow start is over
            if (sliding_window != 1) {
                sliding_window = sliding_window*backoff/(backoff+1); // decrease the packoff after
            }
            // update the timeouts
            timeout_sec = (timeout_usec*2)/1000000;
            timeout_usec = (timeout_usec*2)%1000000;
        } 

        if (!more_packets && p_ack+1 == p_created) { 
            // if there are no more packets we are waiting for
            break; // break to send final EOF packet
        }
    }

    // create the final packet
    char *packet = get_final_packet();
    mylog("[send eof]\n");

    // send it 3 times to deal with potential drops/issues
    if (sendto(sock, packet, sizeof(header)+sizeof(char), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
        perror("sendto");
        exit(1);
    }
    if (sendto(sock, packet, sizeof(header)+sizeof(char), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
        perror("sendto");
        exit(1);
    }
    if (sendto(sock, packet, sizeof(header)+sizeof(char), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
        perror("sendto");
        exit(1);
    }

    // free, log completion, and return
    free(packet);
    for (i = 0; i < WINDOW_SIZE; i++) { // free buffers for sent packets
        free(packets[i]);
    }
    mylog("[completed]\n");

    return 0;
}
