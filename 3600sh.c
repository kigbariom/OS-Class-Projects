/*
 * CS3600, Spring 2014
 * Class Project
 * Conor Golden & Arjun Rao
 * (c) 2013 Alan Mislove
 *
 */

#include "3600sh.h"
#include <stdio.h>
#include <stdlib.h>

#define USE(x) (x) = (x)

// pre defined strings with error codes
char* ERR_ESCAPE = "Error: Unrecognized escape sequence.";
char* ERR_SYNTAX = "Error: Invalid syntax.";
char* ERR_IOR = "Error: Unable to open redirection file.";

//sets the pid of the last background process for use later
pid_t bgp_final = 0;

int main(int argc, char *argv[]){
    // Code which sets stdout to be unbuffered
    // This is necessary for testing; do not change these lines
    USE(argc);
    USE(argv);
    setvbuf(stdout, NULL, _IONBF, 0);
    
	//allocates memory for prompt
	char *username = getenv("USER");
    char* dir = (char *)malloc(1024);
    getcwd(dir, 1023);
    char* hostname = (char *)malloc(1024);
    gethostname(hostname, 1023);

 	char temparray[1024];
	const int tempsize = 1024;

	while (1) {
		//prints the prompt
		printf("%s@%s:%s> ", username ,hostname, dir);
        if(fgets(temparray, tempsize, stdin)) {
            if(execute(temparray))
                break;
        }
        else {
            break;
        }
        if (feof(stdin))
            break;
	}

    printf("So long and thanks for all the fish!\n");

    //Waits for backgrounds process to end
    if(bgp_final)
        waitpid(bgp_final, NULL, 0); 

    return 0;
}

// returns zero once there are no  more next words
// buffer is the entire string, next points to the new cmd, cmd is the first command in the array, and length is the length of the cmd
int organize_args(char *buffer, int *next, char *cmd, int *length) {
    static int amps = 0;
    if (buffer[0] == '\0' ) {
         amps = 0;
         return 0;
    }

    //ignores spaces and tabs
    int i = 0;
    while(buffer[i] != '\0')  {
        if (buffer[i] == ' ' || buffer[i] == '\n'  || buffer[i] == '\t' ) {
           i++;
           continue;
        }
        else
            break;
    }
    //empty input case
    if (buffer[i] == '\0' ) {
         amps = 0;
         return 0;
    }
    int j = 0;
    while(buffer[i]) {
		//catch escape characters
        if (buffer[i] == '\\') {
             if(buffer[i+1] == ' ' || buffer[i+1] == 't' || buffer[i+1] ==  '&' ||  buffer[i+1] == '\\' ) {
                if (buffer[i+1] == 't')
                    cmd[j++]= '\t';
                else
                    cmd[j++]=buffer[i+1];
                i+=2;
                continue;
             }
             else {
                printf("%s\n", ERR_ESCAPE);
                amps = 0;
                return -1;
             }
        }
        if (buffer[i] == ' ' || buffer[i] == '\n'  || buffer[i] == '\t' )
            break;  //cmd finished
        else {
            // background task detected
            if( buffer [i] == '&')
                 amps++; //increment ampersand counter
            cmd[j++] = buffer[i++];
        }
     }
    if(amps)  {
        // nothing can be detected after amps
        if( cmd[j-1] != '&' || amps > 1) {
            printf("%s\n", ERR_SYNTAX);
            amps  = 0;
            return -1;
        }
        if (j > 1) {
            amps = 0;
            j--; i--;
        }
    }
    cmd[j] = '\0';

    *length = j;
    if(*length == 0) {
        amps = 0;
        return 0;
    }
    *next = i;

    return 1;
}

/**
takes in arglist and processes it for io redirection commands and removes them from the argument list
re-populates the argument list

returns non-zero for errors
*/
int io_redirection(char **arglist, int *argn, char *infile, char *outfile, char *errfile) {
    int i; 
	int err = 0; 
    int n = *argn;
    int redirection_found = 0; //io redirection flag
    int infound = 0, outfound = 0, errfound = 0;  //which type of io redirection

    // io_cmds array for io redirection commands
	char **io_cmds = (char **)malloc(sizeof(char **) * 1024);
    int j = 0;

    for( i = 0; i < n; i++) {
        if (!strcmp(arglist[i], "<"))
        {
            if(infound || (i == (n -1))) { //if there is a "<" it can't be the last word
                err = 1;
                break;
            }
            else {
                if(!strcmp(arglist[i+1], "<" ) || !strcmp(arglist[i+1], ">" ) || !strcmp(arglist[i+1], "2>" )) {
                    err = 1;
                    break;
                }

                strcpy( infile, arglist[i + 1]);
                infound = 1;
                free(arglist[i]);
                free(arglist[i+1]);
                i++; 
                redirection_found = 1; //move forward because next word is file
                continue;
            }
        }
        if (!strcmp(arglist[i], ">"))
        {
            if(outfound || (i == (n -1))) {
                err = 1;
                break;
            }
            else {
                if(!strcmp(arglist[i+1], "<" ) || !strcmp(arglist[i+1], ">" ) || !strcmp(arglist[i+1], "2>" )) {
                    err = 1;
                    break;
                }
                strcpy( outfile, arglist[i + 1]);
                outfound = 1;
                free(arglist[i]);
                free(arglist[i+1]);
                i++;    
                redirection_found = 1;
                continue;
            }
        }
        if (!strcmp(arglist[i], "2>"))
        {
            if(errfound || (i == (n -1))) {
                err = 1;
                break;
            }
            else {
                if(!strcmp(arglist[i+1], "<" ) || !strcmp(arglist[i+1], ">" ) || !strcmp(arglist[i+1], "2>" )) {
                    err = 1;
                    break;
                }
                strcpy( errfile, arglist[i + 1]);
                errfound = 1;
                free(arglist[i]);
                free(arglist[i+1]);
                i++; 
                redirection_found = 1;
                continue;
            }
        }

        // no arguments after IO redirection
        if(redirection_found) {
            err = 1;
            break;
        }

        io_cmds[j++] = arglist[i];
    }

    if (j == 0) // j==0 is error
        return 1;

    if(err) {
        // free arglist after i
        for(;i<n;i++)
            free(arglist[i]);
        return err;
    }

	//new length of arglist
	//add the elements of io commands into arglist and free io commands
    *argn = j;
    arglist[j] = NULL;
    while(j--) {
        arglist[j] = io_cmds[j];
    }
    free(io_cmds);
    return 0;
}

//catches any error and returns non-zero if one is found
int handle_io_errors(char *infile, char *outfile, char *errfile) {
    int fdin, fdout, fderr;
    if(strlen(infile)) {
        fdin = open(infile, O_CREAT | O_RDWR | O_TRUNC,  S_IRUSR | S_IWUSR);
        //open returns -1 if failed
        if(fdin == 0) {
            printf("%s\n", ERR_IOR);
            return -1;
        }
        int errorcheck = dup2(fdin, 0);
        if (errorcheck == -1){
            printf("%s\n", ERR_IOR);
            return -1;
        }
    }

    if(strlen(outfile)) {
        fdout = open(outfile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
       
        if(fdout == 0) {
            printf("%s\n", ERR_IOR);
            return -1;
        }
		// duplicates the file descriptor
        int errorcheck = dup2(fdout, 1);
        if (errorcheck == -1){
            printf("%s\n", ERR_IOR);
            return -1;
        }
    }

    if(strlen(errfile)) {
        fderr = open(errfile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
      
        if(fderr == 0) {
            printf("%s\n", ERR_IOR);
            return -1;
        }
        int errorcheck = dup2(fderr, 2);
        if (errorcheck == -1){
            printf("%s\n", ERR_IOR);
            return -1;
        }
    }

   
    return 0;
}

//populates a 2d array with arguments and commands
//returns non-zero if execute fails
//takes in cmd which is the argument, and forks that command
//waits for processes to end or not if we have amps
//this way we can detect amps and fork the processes in the same command
int execute(char *cmd)
{
    char *arg = cmd;
    char word[1024];
    int ends = 0, length = 0;

	char **arglist = (char **)malloc(sizeof(char **) * 1024);
    int argn = 0;

    int errorcheck;
	//runs organize args
    while ((errorcheck = organize_args(arg, &ends, word, &length))) { //& because organize_arg takes in a pointer to ints
		if (errorcheck == -1) { // organize_args returned an error, input must be invalid
            clear_memory(arglist, argn);
            free(arglist);
            return 0; 
        }
		char *temp = (char *)malloc(sizeof(char)* 1024);
        int i;
        for(i= 0; i < length; i++) {
           temp[i] = word[i];
        }
        temp[i] = '\0';

        arg = arg + ends;
        arglist[argn++] = temp;
    }
    arglist[argn] = NULL;

    if(argn == 0) {
        free(arglist);
        return 0;
    }

    // if the cmd was exit, quit
    if(!strcmp(arglist[0], "exit")) {
        clear_memory(arglist, argn);
        free(arglist);
        return 1;
    }

    int background = 0;
    if(arglist[argn-1][0] == '&') {
        background = 1;
        //take away amps arguement after it is found
        argn--;
        arglist[argn] = NULL;
    }
    background = 2*background;

	//these files must be initialized as empty for io_redirection
    char infile[1024] = "", outfile[1024] = "", errfile[1024] = "";
    int iof = io_redirection(arglist, &argn, infile, outfile, errfile);

    if(iof)
    {
        free(arglist);
        printf("%s\n", ERR_SYNTAX);
        return 0;
    }

	pid_t child_pid;
	/* fork another process */
	child_pid = fork();
	if (child_pid == 0) {
	    
		//if there is a background task, setpgid
        if (background) {
            setpgid(0,0);
		}

		//check for any IO errors
        int errorcheck = handle_io_errors(infile, outfile, errfile);
        if (errorcheck) {
            exit(1);
        }

		//runs execvp and checks for errors
		if (execvp(arglist[0], arglist) == -1) {
            if (errno == ENOENT) //file missing error
                printf("Error: Command not found.\n");
            else if (errno == EACCES) //permission error
                printf("Error: Permission denied.\n");
            else
                printf("Error: %s\n", strerror(errno));
		}
		exit(1);
		exit(1);
	} else {
	    // wait for the child if it is not a background process
	    if (background) {
            waitpid(-1, NULL, WNOHANG);// WNOHANG regardless of the child finishing
            bgp_final = child_pid;//wait for this to finish in main so we know it is over
	    }
		else
			//wait until the child is complete
            waitpid(child_pid, NULL, 0);
		}
    return 0;
}

//helper method to free the whole arglist easily
void clear_memory(char **arglist, int argn) {
    int i;
    for(i = 0; i < argn; i++)
        free(arglist[i]);
    return;
}

//exits the shell and prints the special message
void do_exit() {
  printf("So long and thanks for all the fish!\n");
  exit(0);
}