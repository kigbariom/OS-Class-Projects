/*
 * CS3600, Spring 2014
 * Class Project
 * Conor Golden & Arjun Rao
 * (c) 2013 Alan Mislove
 *
 */


#include <assert.h>
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

int main() {
    /**
     * I've included some basic code for opening a UDP socket in C, 
     * binding to a empheral port, printing out the port number.
     * 
     * I've also included a very simple transport protocol that simply
     * acknowledges every received packet.  It has a header, but does
     * not do any error handling (i.e., it does not have sequence 
     * numbers, timeouts, retries, a "window"). You will
     * need to fill in many of the details, but this should be enough to
     * get you started.
     */

    // first, open a UDP socket  
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // next, construct the local port
    struct sockaddr_in out;
    out.sin_family = AF_INET;
    out.sin_port = htons(0);
    out.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &out, sizeof(out))) {
        perror("bind");
        exit(1);
    }

    struct sockaddr_in tmp;
    int len = sizeof(tmp);
    if (getsockname(sock, (struct sockaddr *) &tmp, (socklen_t *) &len)) {
        perror("getsockname");
        exit(1);
    }

    mylog("[bound] %d\n", ntohs(tmp.sin_port));

    // wait for incoming packets
    struct sockaddr_in in;
    socklen_t in_len = sizeof(in);

    // construct the socket set
    fd_set socks;

    // construct the timeout
    struct timeval t;
    t.tv_sec = 15;
    t.tv_usec = 0;

    // our receive buffer
    int buf_len = 1500;
    void *buf = malloc(buf_len); 
    
    char *data_buf = (char *) malloc((WINDOW_SIZE * buf_len) * sizeof(char));
    unsigned int *bufferlength = (unsigned int*) malloc(WINDOW_SIZE * sizeof(unsigned int)); 

    // current counter
    unsigned int current_packet = 1;
   
    // wait to receive, or for a timeout
    while (1) {
        FD_ZERO(&socks);
        FD_SET(sock, &socks);
        if (select(sock + 1, &socks, NULL, NULL, &t)) {
            int received;
            if ((received = recvfrom(sock, buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
                perror("recvfrom");
		//free all allocated memory
                free(buf);
                free(data_buf);
                free(bufferlength);
                exit(1);
            }
            // get the checksum
            int dataLen = ntohs(((header *) buf)->length);
            unsigned char expected_checksum = get_checksum((char *)buf, get_data(buf), dataLen); 
            header *myheader = get_header(buf);
            char *data = get_data(buf);
            if (myheader->magic == MAGIC) {
                // matches the magic number
                if (expected_checksum != *((unsigned char *)(data-1))) {
                    // checksum does not match
                    mylog("[recv checksum error detected]");
                    continue;
                }
		//If the sequnece matches, progress the sequence number
                if ((int)myheader->sequence == current_packet) {
                    // checks the sequence, writes it to memory
                    write(1, data, myheader->length);
                    current_packet++;

                    // find the buffer index by modding the packet and the set window size
                    unsigned int buffer_index = current_packet % WINDOW_SIZE;
                    
                    while(bufferlength[buffer_index] > 0) {
                        write(1, &data_buf[buffer_index * buf_len], bufferlength[buffer_index]);
                        
                        // change the legnth to zero
                        bufferlength[buffer_index] = 0;
                        
                        // Increment the packet
                        current_packet++;
                        buffer_index = current_packet % WINDOW_SIZE;
                    }
                }
                else {
                    // Is not the current packet
                    // Add to buffer and set length
                    int buffer_index = (int)myheader->sequence % WINDOW_SIZE;
                    
                    if (bufferlength[buffer_index] == 0) {
                        bufferlength[buffer_index] = myheader->length;
                        memcpy(&data_buf[buffer_index*buf_len], data, myheader->length);
                    }
                }

                mylog("[recv data] %d (%d) %s\n", (int)myheader->sequence, myheader->length, "ACCEPTED (in-order)");
                mylog("[send ack] %d\n", current_packet-1);

                // creates a packet to respond
                void *packet = malloc(sizeof(header) + sizeof(char));
               
                // add the header memcpy
                header *responseheader = make_header((short)current_packet - 1, 0, myheader->eof, 1, ntohl(myheader->time));
                memcpy(packet, responseheader, sizeof(header));

                // add the checksum
                unsigned char *checksum = malloc(sizeof(char));
                *checksum = get_checksum((char *)responseheader, NULL, 0);
                memcpy(((char *) packet) +sizeof(header), (char *)checksum, sizeof(unsigned char));

                free(responseheader);
                free(checksum);
               
                if (sendto(sock, packet, sizeof(header) + sizeof(char), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0) {
                    perror("sendto");
                    free(buf);
                    free(data_buf);
                    free(bufferlength);
                    free(packet);
                    exit(1);
                }
                
		// if the end of the file is reached
                if (myheader->eof) {
                    mylog("[recv eof]\n");
                    mylog("[completed]\n");
		    //free allocated memory
                    free(buf);
                    free(data_buf);
                    free(bufferlength);
                    free(packet);
                    exit(0);
                }

                free(packet);
            } else {
                mylog("[recv corrupted packet]\n");
            }
        } else {
            mylog("[error] timeout occurred\n");
            free(buf);
            free(data_buf);
            free(bufferlength);
            exit(1);
        }
    }
 
    free(buf);
    free(data_buf);
    free(bufferlength);
    //recv success
    return 0;
}
