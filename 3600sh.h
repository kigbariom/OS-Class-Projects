/*
 * CS3600, Spring 2014
 * Class Project
 * Conor Golden & Arjun Rao
 * (c) 2013 Alan Mislove
 *
 */

#ifndef _3600sh_h
#define _3600sh_h

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int organize_args(char *str, int *ends, char *word, int *length);
int io_redirection(char **argv, int *argn, char *infile, char *outfile, char *errfile);
int handle_io_errors(char *infile, char *outfile, char *errfile);
int execute(char *cmd);
void clear_memory(char **argv, int argn);
void do_exit();

#endif 
