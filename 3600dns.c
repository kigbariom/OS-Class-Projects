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

#include "3600dns.h"

/**
 * This function will print a hex dump of the provided packet to the screen
 * to help facilitate debugging.  In your milestone and final submission, you 
 * MUST call dump_packet() with your packet right before calling sendto().  
 * You're welcome to use it at other times to help debug, but please comment those
 * out in your submissions.
 *
 * DO NOT MODIFY THIS FUNCTION
 *
 * data - The pointer to your packet buffer
 * size - The length of your packet
 */
static void dump_packet(unsigned char *data, int size) {
    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((unsigned int)p-(unsigned int)data) );
        }
            
        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

void dns_format(unsigned char* dns,unsigned char* host)
{
  int lock=0 , i;

  strcat((char*)host,".");

  for(i=0 ; i<(int)strlen((char*)host) ; i++)
  {
    if(host[i]=='.')
    {
      *dns++=i-lock;
      for(;lock<i;lock++)
      {
        *dns++=host[lock];
      }
      lock++; //or lock=i+1;
    }
  }
  *dns++='\0';
}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a socket in C, sending
   * a UDP packet, and then receiving a response (or timeout).  You'll 
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */

  // process the arguments [MILESTONE]
  if (argc != 3 && argc != 4) {
    printf("ERROR\tInvalid Arguments\n");
    return 0;
  }

  char input_type = 1;
  if (argc == 4) {
        if (!strcmp(argv[1],"-mx")){
            input_type = 15;
        } else if (!strcmp(argv[1],"-ns")){
            input_type = 2;
        } else {
            return -1;
        }
        argc = 1;
  } 
  else {
    argc = 0;
  }  

  int port = 53; // defaults to 53
  char *ip_address = ++argv[1]; //IP address of the DNS server, in a.b.c.d format
  char *colon = strchr(ip_address, ':'); //port represented as a string
  if(colon){
    *colon = '\0';
    port = atoi(++colon); // convert the parsed string of the port into an int
  } 

  // construct the DNS request  [MILESTONE]
  unsigned char packet[65536],*qname; //Where we store our packet and name
  dns_header *header = NULL; //Where we store our header
  dns_question *question = NULL; //Where we store our question type

  // assign the header struct to the data in packet (first part)
  header=(dns_header*)&packet;

  // HEADER----------------------------
  header->id = (unsigned short)htons(0x0539); //Query ID
  header->qr = 0; //This is a query
  header->opcode = 0; //This is a standard query
  header->aa = 0; //Not Authoritative
  header->tc = 0; //This message is not truncated | not used
  header->rd = 1; //Recursion Desired left as one
  header->ra = 0; //Recursion not available | not used
  header->z = 0; //Reserved for later use
  header->ad = 0; //Not authenticated data
  header->cd = 0; //checking disabled
  header->rcode = 0; //response code
  header->q_count = htons(0x0001); //we have only 1 question
  header->ans_count = 0; //number of answer entries
  header->auth_count = 0; //number of authority entries
  header->add_count = 0; //number of resource entries

  // QUESTION--------------------------
  
  // assign the qname to come right after the header in packet
  qname =(unsigned char*)&packet[sizeof(dns_header)];
  dns_format(qname, (unsigned char *)argv[2]);

  // assign the rest of the question to come after qname in packet
  question = (dns_question*)&packet[sizeof(dns_header) + (strlen((const char*)qname) + 1)];

  switch (input_type){

        case 1: 
            question->qtype = htons(0x0001); //IP address type, one means IPv4
            break;
        case 15:
            question->qtype = htons(0x000f);
            break;
        case 2:
             question->qtype = htons(2);
            break;
    }
  question->qclass = htons(0x0001);

  // ANSWER----------------------------

  // send the DNS request (and call dump_packet with your request)
  int packet_len = sizeof(dns_header) + strlen((char *)qname) + 1 + sizeof(dns_question);
  dump_packet(packet, packet_len);
  
  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the destination address
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(port); //Port
  out.sin_addr.s_addr = inet_addr(ip_address);  //DNS Server

  if (sendto(sock, packet, packet_len, 0, (struct sockaddr *)&out, sizeof(out)) < 0) { //Points to packet and packet len
    // an error occurred
    printf("ERROR\tsendto failed.\n");
    return 0;
  }

  // wait for the DNS reply (timeout: 5 seconds)
  struct sockaddr_in in;
  socklen_t in_len;

  // construct the socket set
  fd_set socks;
  FD_ZERO(&socks);
  FD_SET(sock, &socks);

  // construct the timeout
  struct timeval t;
  t.tv_sec = 5; //timeout in seconds
  t.tv_usec = 0;

  // wait to receive, or for a timeout
  if (select(sock + 1, &socks, NULL, NULL, &t)) {
    if (recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&in, &in_len) < 0) {
      // an error occured
      printf("ERROR\trecvfrom failed.\n");
      return 0;
    }
  } else {
    // a timeout occurred
    printf("NORESPONSE\n");
    return 0;
  }

  // copy from the packet into the header structure
  memcpy(header, packet, sizeof(dns_header));
  header->id = ntohs(header->id);
  header->q_count = ntohs(header->q_count);
  header->ans_count = ntohs(header->ans_count);
  header->auth_count = ntohs(header->auth_count);
  header->add_count = ntohs(header->add_count);

  // response code of 3 indicates not found
  if(header->rcode == 3) {
    printf("NOTFOUND\n");
    return 0;
  }


  

  //MAKE QUERIES ---------------------------------

  int shift = 12; //bits to shift by
  dns_query queries[header->q_count];
  for(int i = 0; i < header->q_count; i++) {
    char *qname = malloc(255);
    int qindex = 0; //index of the question
    char count = 0; //move through the header
    while(packet[shift] != '\0'){
      if(count == 0){
        count = packet[shift];
        qname[qindex] = '.';
      }else {
        qname[qindex] = packet[shift];
        count --;
      }
      qindex ++;
      shift ++;
    }
    qname[qindex] = packet[shift];
    // initialize the name field by allocating the index+1
    queries[i].name = (unsigned char *) malloc(qindex + 1);
    strncpy((char *)queries[i].name, (char *)qname, qindex);
    free(qname);
    // get rest of query information
    dns_question q;
    q.qtype = (packet[shift+1] << 8) + packet[shift+2];
    q.qclass = (packet[shift+3] << 8) + packet[shift +4];
    queries[i].ques = &q;
    shift += 5;
  }

  //MAKE ANSWERS ---------------------------------

  dns_record rec[header->ans_count];
  for(int i = 0; i < header->ans_count; i++){
    unsigned char *temp = (unsigned char *) malloc(255);
    int nindex = 0; //answer shift count
    unsigned char count = 0;
    int index_restore = 0;
    while(packet[shift] != '\0'){
      if(count == 0){
        // point to the data if count is zero
        if(packet[shift] & 0x10000000){
          unsigned short ptr = (packet[shift] << 8) + packet[shift+1];
          ptr &= 0x0011111111111111; //bitwise & on the pointer in the packet
          if (!index_restore) index_restore = shift + 1;
          //found the label
	  shift = ptr;
          continue;
        }
        count = packet[shift];
        temp[nindex] = '.';
      } else {
        temp[nindex] = packet[shift];
        count --;
      }
      nindex ++;
      shift ++;
    }
    temp[nindex] = packet[shift]; // null terminator
    // allocate the string to the answers array and free temp
    rec[i].name = malloc(nindex + 1);
    strncpy((char *)rec[i].name, (char *)temp, nindex);
    free(temp);
    if(index_restore) shift = index_restore;
    dns_answer *rd = (dns_answer *) malloc(sizeof(dns_answer));
    rd->type = (packet[shift] << 8) + packet[shift + 1];
    rd->_class = (packet[shift + 2] << 8) + packet[shift + 3];
    rd->ttl = (packet[shift +4] << 24) + (packet[shift + 5] << 16) + (packet[shift + 6] << 8) + packet[shift + 7];
    rd->data_len = (packet[shift + 8] << 8) + packet[shift + 9];
    rec[i].resource = rd;
    shift += 10;

    // parse the rdata in the dns_answer field
    if (rd->type == 1){
     unsigned char rdata[4];
     for(int j = 0; j < 4; j++){
       rdata[j] = packet[shift + j];
     }
     rec[i].rdata = rdata;
     shift += 4;
   }
   if (rd->type == 5) { //type 5 is cname
     temp = malloc(255);
     nindex = 0;
     count = 0;
     index_restore = 0;
      while(packet[shift] != '\0'){
        // find label length if count == 0
        if(count == 0){
          // check for a pointer
          if(packet[shift] & 0xc0){
            unsigned short ptr = (packet[shift] << 8) + packet[shift+1];
            ptr &= 0x3fff;
            if(!index_restore) index_restore = shift + 1;
            shift = ptr;
            continue;
          }
          count = packet[shift];
          temp[nindex] = '.';
        } else {
          temp[nindex] = packet[shift];
          count --;
        }
        nindex ++;
        shift ++;
      }
      temp[nindex] = packet[shift]; // null terminator
      temp += 1;
    // allocate the rdata to the answers array and free temp
    rec[i].rdata = malloc(nindex);
    strncpy((char *)rec[i].rdata, (char *)temp, nindex - 1);
    free(--temp);
    if(index_restore) shift = index_restore;
   }
  }

  // the rest of the packet is not used
  
  // print formatted output
  packet_len += sizeof(dns_answer) - 2;
  short preference = 0;
  for(int i = 0; i < header->ans_count; i++){
    if (rec[i].resource->type == 1){
      printf("IP\t%d.%d.%d.%d\t", rec[i].rdata[0], rec[i].rdata[1], rec[i].rdata[2], rec[i].rdata[3]);
    }
    if (rec[i].resource->type == 5){ //type 5 is cname
      printf("CNAME\t%s\t", rec[i].rdata);
    }
    else if (rec[i].resource->type == 2 ) { //type 2 is NS
      printf("NS\t%s",rec[i].rdata);
    }
    else if (rec[i].resource->type == 15 ) { // type 15 is MX
      memcpy(&preference,packet+packet_len,sizeof(short));
      packet_len+=sizeof(preference);  
      preference = ntohs(preference); 
      printf("MX\t%s\t%d",rec[i].rdata,preference);
    }
    if(header->aa) printf("auth\n");
    else printf("nonauth\n");
  }

  return 0;
}

