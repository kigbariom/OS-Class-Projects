/*
 * CS3600, Spring 2014
 * Class Project
 * Conor Golden & Arjun Rao
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600SENDRECV_H__
#define __3600SENDRECV_H__

#include <stdio.h>
#include <stdarg.h>

#define WINDOW_SIZE 2000
#define SENDER_TIMEOUT_SEC 3
#define SENDER_TIMEOUT_MICRO 00000 // base timeout
#define DUPLICATE_ACKS 5 // # of duplicate ACKs to fast retransmit
#define RTT_DECAY 0.33 // decay factor for old moving average values
#define RTT_MULT 1.0 // multiplication factor for timeout error
#define CWD_SCALE_FACTOR 1.0 // scale factor for congestion avoidance window increases

typedef struct header_t {
  unsigned int magic:14;
  unsigned int ack:1;
  unsigned int eof:1;
  unsigned short length;
  unsigned short sequence;
  unsigned int time;
} header;

unsigned int MAGIC;

unsigned char get_checksum(char *header, char *data, int length);
void dump_packet(unsigned char *data, int size);
header *make_header(short sequence, int length, int eof, int ack, unsigned int time);
header *get_header(void *data);
char *get_data(void *data);
char *timestamp();
void mylog(char *fmt, ...);

#endif

