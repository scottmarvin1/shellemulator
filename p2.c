/* Gregory Scott Marvin
 * Program 4 Due November 29, 2019  
 * CS570 Fall 2019 Dr. John Carroll
 * San Diego State University
 */

/*Include statements to perform all the necessary system calls*/
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "p2.h"

/*Constants used in the program*/
#define MAXARGS 20
#define MAXHISTORY 10
#define BIGBUFFER 5120

/*Global flags and the newargv array used throughout the program*/
char *newargv[MAXARGS];
char *outfile = NULL;
char *infile = NULL;
char *word = NULL;
int newargc = 0;
int childcommandindex = 0;
int replace = 0;
int historyindex = 0;
int inputset = 0;
int outputset = 0;
int outputerrorset = 0;
int laypipe = 0;
int appendtofile = 0;
int ambiguityfound = 0;
int pipeerror = 0;
int backgroundjob = 0;
int commentindex = -1;
extern int pipeescape; /* int pipeescape is defined in getword.c */

/*Function prototype declarations*/
int parse(char *buff);
void clearnewarg();
void signalcatch(int signum);

/*struct used to save the flags, arguments and input/output files of the previous command*/
struct Command {
    char oldbuffer[BIGBUFFER];
    char *oldargv[MAXARGS];
    char *oldoutfile;
    char *oldinfile;
    int oldargc;
    int oldchildcommandindex;
    int oldinputset;
    int oldoutputset;
    int oldoutputerrorset;
    int oldappendtofile;
    int oldlaypipe;
    int oldambiguityfound;
    int oldpipeerror;
    int oldbackgroundjob; 
};

main(int argc, char *argv[]) {
    int wordcount; 
    char buffer[BIGBUFFER];
    int filedes[2];
    int kpid;
    int gkpid;
    int inputfd;
    int outputfd;
    int i;
    int length;
    int commandcount = 1;
    struct Command history[MAXHISTORY];

    /*Catch any SIGTERM signal sent by the overarching process when one of the children processes are
     *killed. The signals are caught by the custom signal handler function signalcatch.*/
    (void) signal(SIGTERM, signalcatch);    

    /*MAKE SURE that the program is moved to it's own process group so it runs independent of other program
     *process groups to not kill the login shell and the autograder.*/
    if (setpgid(0, 0) < 0) {
        perror("Error setting the process group ID");
        exit(errno);
    }

    /*Check to see if the program was specified with a script to be run and load it into the program. Of course
     *errors should be checked for with the system calls.*/
    if (argc > 1) {
        if ((inputfd = open(argv[1], O_RDONLY)) < 0) {
            perror("Error with command line arguments");
            exit(errno);
        } 
        if (dup2(inputfd, STDIN_FILENO) < 0) {
            perror("Error copying file descriptor");
            exit(errno);
        }
        if (close(inputfd) < 0) {
            perror("Error closing old file descriptor");
            exit(errno);
        }
    } 
    
    for(;;) {
        /*Reset flags and input/output files for the new line of input.*/
        inputset = 0;
        outputset = 0;
        outputerrorset = 0;
        laypipe = 0;
        childcommandindex = 0;
        replace = 0;
        historyindex = 0;
        appendtofile = 0;
        ambiguityfound = 0;
        pipeerror = 0;
        infile = NULL;
        outfile = NULL;
        wordcount = 0;
        backgroundjob = 0;
        length = 0;
        commentindex = -1;
        pipeescape = 0;

        /*Print out the prompt to indicate to the user that the shell is ready to accept input. 
         *However, if p2 is provided command line arguments (ie run as a script) then the prompt
         *should not printed.*/
        if (argv[1] == NULL) printf("%s%d%s", "%", commandcount, "% ");

        /*Parse the line of input provided by the user, set any flags as needed and populate the 
         *newargv array.*/
        wordcount = parse(buffer);

        /*Terminate program if EOF is reached with an empty line of input.*/
        if (wordcount == -1 && newargc == 0) break;

        /*Check to see if there are some files to redirect to but not command supplied.*/
        if (wordcount == 0 && (inputset | outputset | outputerrorset)) {
            fprintf(stderr, "%s\n", "Error: No command supplied");
            continue;
        }

        /*Check to see if there are redirects provided, but no files associated with them. Due to the way
         *that parse handles the redirects, if no file was provided it will assign the file pointer an empty
         *string for the file name.*/
        if((inputset && (strcmp(infile, "") == 0)) || 
           (outputset && (strcmp(outfile, "") == 0)) || 
           (outputerrorset && (strcmp(outfile, "") == 0))) {
            fprintf(stderr, "%s\n", "Error: Redirect provided with no file");
            continue;
        }

        /*If there is an empty input line but EOF has yet to be reached, reissue prompt for more input*/
        if (wordcount == 0) continue;

        /*If there is an ambiguous command supplied, ie multiple redirects of the same type, provide an
         *error and reissue prompt for more input*/
        if (ambiguityfound) {
            fprintf(stderr, "%s\n", "Error: Ambiguous command supplied");
            continue;
        }

        /*Terminate the program if done is the first word of an input line*/ 
        if (strcmp(newargv[0], "done") == 0) break; 

        /*commandcount should be incremented after all the built-ins are handled as to not inadvertently
         *increase the number of commands run.*/
        commandcount++;

        /*If !! is the first word on an input line, restore the previous command by setting newargv
         *to the previous newargv, setting all flags to the previous command state, and any input or
         *or output file to the previous files. All content for previous command is located in the array
         *element of commandcount - 3. commandcount starts at 1 and is incremented before this action so
         *subtracting three from commandcount provides the correct index for the last command executed.*/
        if (strcmp(newargv[0], "!!") == 0) {
            for (i = 0; i < history[commandcount - 3].oldargc; i++) {
                newargv[i] = history[commandcount - 3].oldargv[i];
            }
            newargc = history[commandcount - 3].oldargc;
            childcommandindex = history[commandcount - 3].oldchildcommandindex;
            infile = history[commandcount - 3].oldinfile;
            outfile = history[commandcount - 3].oldoutfile;
            inputset = history[commandcount - 3].oldinputset;
            outputset = history[commandcount - 3].oldoutputset;
            outputerrorset = history[commandcount - 3].oldoutputerrorset;
            laypipe = history[commandcount - 3].oldlaypipe;
            ambiguityfound = history[commandcount - 3].oldambiguityfound;
            appendtofile = history[commandcount -3].oldappendtofile;
            pipeerror = history[commandcount - 3].oldpipeerror;
            backgroundjob = history[commandcount - 3].oldbackgroundjob;
        }

        /*If p2 was supplied command line arguments, and the commentindex flag was set, then comments
         *located in the supplied script should be ignored. Parse adds these to newargv still so they
         *need to be removed if they do not come from stdin.*/ 
        if (argv[1] != NULL && commentindex > -1) {
            while (newargc >= commentindex) {
                newargv[newargc - 1] = NULL;
                newargc--;
            }
        }
        
        /*If the replace flag is set, then !$ needs to be resolved to the last word of the last command
         *executed. For every occurence of !$ in newargv, the last word will be swapped in. the last 
         *word is stored in a char array and retrieved by accessing the array of structs containing the
         *command history. Using the oldargc - 1 on the oldargv, the appropriate word is retieved. To
         *avoid Segmentation faults, if the laypipe flag is set and the index is equal to the 
         *childcommandindex for the piped command, then continue to the next word as to avoid 
         *referencing a NULL value.*/
        if (replace) {
            word = history[commandcount - 3].oldargv[history[commandcount - 3].oldargc - 1];
            for (i = 0; i < newargc; i++) {
                if (laypipe && i == childcommandindex - 1) continue; 
                if (strcmp(newargv[i], "!$") == 0) {
                    newargv[i] = word;
                }
            }
        }
 
        /*Command History logic. If ![1-9] are provided, then parse the string for the digit after the
         *! character and convert the string representation of the number to an integer for use as an
         *array index in the array of structs of previous commands. Error checking is performed to 
         *ensure the integer provided is less than the number of commands given to avoid Segmentation
         *faults. Additionally, a check is performed to make sure the integer provided is within the 
         *number of max commands rememberred. Currently an integer over 11 will break the program, but
         *adding a modulo 10 to each integer would allow for more commands to be cycled through the 
         *array of structs.*/
        if ((strncmp(newargv[0], "!", 1) == 0) && (strlen(newargv[0]) == 2)){
            word = newargv[0];
            historyindex = atoi(word + 1);
            if (commandcount - 1 <= historyindex || historyindex > 10) {
                fprintf(stderr, "%s\n", "Error: Invalid history index");
                continue;    
            }
            if (historyindex >= 0 && historyindex < 10) {
                for (i = 0; i < history[historyindex - 1].oldargc; i++) {
                    newargv[i] = history[historyindex - 1].oldargv[i];
                }
                newargc = history[historyindex - 1].oldargc;
                childcommandindex = history[historyindex - 1].oldchildcommandindex;
                infile = history[historyindex - 1].oldinfile;
                outfile = history[historyindex - 1].oldoutfile;
                inputset = history[historyindex - 1].oldinputset;
                outputset = history[historyindex - 1].oldoutputset;
                outputerrorset = history[historyindex - 1].oldoutputerrorset;
                laypipe = history[historyindex - 1].oldlaypipe;
                ambiguityfound = history[historyindex - 1].oldambiguityfound;
                appendtofile = history[historyindex - 1].oldappendtofile;
                pipeerror = history[historyindex - 1].oldpipeerror;
                backgroundjob = history[historyindex - 1].oldbackgroundjob;
            }
        }

        /*If & is the last word of an input line command needs to be a background job. Remove the & from
         *newargv to void causing errors with the execvp system call*/
        if (strcmp(newargv[newargc - 1], "&") == 0) {
            newargv[newargc - 1] = NULL;
            backgroundjob = 1;
            newargc--;
        }

        /*Check if cd is the first word from parse. If no arguments are provided, it should assume the users
         *home directory is where it should go. Otherwise it will go to the supplied directory. If there are
         *multiple arguments provided for cd, indicate to the user that it is an invalid command.*/
        if (strcmp(newargv[0], "cd") == 0) {
            if (newargc >= 3) {
                fprintf(stderr, "%s\n", "Error: too many arguments for cd");
                continue;
            }
            if (newargv[1] == NULL) {
                chdir(getenv("HOME"));
            }
            else {
                chdir(newargv[1]);
            }
            goto update;
        }
  
        /*Flush out the parents output buffers before forking a child. This ensures that the child does
         *not inherit any unnecessary output from their parent.*/
        fflush(stdout);
        fflush(stderr);
        /*Parent forks a child so the supplied command runs as its own process allowing the shell to continue
         *its execution.*/
        kpid = fork();
        if (kpid < 0) {
            fprintf(stderr, "%s\n", "Unable to fork...");
            continue;
        } 
        /*A pid returned from fork that is less than 0 indicates the process is the parent process.*/
        else if (kpid != 0) {
            if (backgroundjob) printf("%s [%d]\n", newargv[0], kpid);
            else{ 
                /*Parent waits for it's children to die, if the parent and child's pid is the same the wait is over.*/
                for (;;) {
                    pid_t pid;
                    pid = wait(NULL);
                    if (pid == kpid) {
                        break;
                    }
                }
            }
        }
        /*A pid returned that is equal 0 indicates the process is the child process. The processes should do
         *as good children do, and do the heavy lifting for the parent. This includes open files, copying
         *the file descriptors, closing the old file descriptors and executing the supplied command while
         *making sure the system calls did not result in errors.*/
        else {
            /*If the input redirect was set, open the supplied file in read only for the input with the 
             *permissions of read and write for the user (equivalent to the octet 600). Check for failure
             *on the open, dup2, and close system calls.*/
            if (inputset) {
                /*If the file name is !$ then the filename needs to be replaced with the last word of the 
                 *last command executed.*/
                if (strcmp(infile, "!$") == 0) {
                    infile = history[commandcount - 3].oldargv[history[commandcount - 3].oldargc - 1];
                }
                if ((inputfd = open(infile, O_RDONLY, S_IRUSR | S_IWUSR)) < 0) {
                    perror("Error with input file");
                    exit(errno);
                }
                if (dup2(inputfd, STDIN_FILENO) < 0) {
                    perror("Error copying file descriptor");
                    exit(errno);
                }
                if (close(inputfd) < 0) {
                    perror("Error closing old file descriptor");
                    exit(errno);
                }
            }
            else {
                /*If a background job has been specified with an & as the last word on the input line, then
                 *redirect the child's input to /dev/null.*/
                if (backgroundjob) {
                    if ((inputfd = open("/dev/null", O_RDONLY)) < 0) {
                        perror("Error with /dev/null");
                        exit(errno);
                    }
                    if (dup2(inputfd, STDIN_FILENO) < 0) {
                        perror("Error copying file descriptor");
                        exit(errno);
                    }
                    if (close(inputfd) < 0) {
                        perror("Error closing old file descriptor");
                        exit(errno);
                    }
                }
            }

            /*If the output redirect was supplied, DON'T open the provided file if it exists (O_EXCL flag),
             *create the file if it doesn't exist (O_CREAT flag), and open in write only mode for output. The
             *file should be opened with read and write permissions for the user (equivalent to the octet 600).
             *The exception to the exclusive open is if the appendtofile flag is set. If this is the case, then
             *the file should only be opened IF the file exists.*/
            if (outputset) {
                /*If the file name is !$ then the filename needs to be replaced with the last word of the 
                 *last command executed.*/
                if (strcmp(outfile, "!$") == 0) {
                    outfile = history[commandcount - 3].oldargv[history[commandcount - 3].oldargc - 1];
                }    
                if (appendtofile) {
                    if ((outputfd = open(outfile, O_APPEND | O_WRONLY)) < 0 ) {
                        perror("Error with output file");
                        exit(errno);
                    }
                }
                else {
                    if ((outputfd = open(outfile, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR)) < 0) {
                        perror("Error with output file");
                        exit(errno);
                    }
                }
                if (dup2(outputfd, STDOUT_FILENO) < 0) {
                    perror("Error copying file descriptor");
                    exit(errno);
                }
                if (close(outputfd) < 0) {
                    perror("Error closing old file descriptor");
                    exit(errno);
                }
            }

            /*If the output with error redirect was supplied, DON'T open the provided file if it exists, create
             *the file if it doesn't exist, and open it in write only mode for output. The file should be opened
             *with read and write permissions for the user. The exception to this is if the appendtofile flag is
             *set. If this is the case then the file should only be opened IF the file exists.*/
            if (outputerrorset) {
                /*If the file name is !$ then the filename needs to be replaced with the last word of the 
                 *last command executed.*/
                if (strcmp(outfile, "!$") == 0) {
                    outfile = history[commandcount - 3].oldargv[history[commandcount - 3].oldargc - 1];
                }    
                if (appendtofile) {
                    if ((outputfd = open(outfile, O_APPEND | O_WRONLY)) < 0) {
                        perror("Error with output file");
                        exit(errno);
                    }
                }
                else {
                    if ((outputfd = open(outfile, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) < 0) {
                        perror("Error with output file");
                        exit(errno);
                    }
                }
                if (dup2(outputfd, STDOUT_FILENO) < 0) {
                    perror("Error copying file descriptor");
                    exit(errno);
                }
                if (dup2(outputfd, STDERR_FILENO) < 0) {
                    perror("Error copying file descriptor");
                    exit(errno);
                }
                if (close(outputfd) < 0) {
                    perror("Error closing old file descriptor");
                    exit(errno);
                }
            }

            /*If the laypipe flag is set, then the plumbing for vertical piping should be set up. For
             *vertical piping, the child needs to fork another child (the grandchild of the parental p2
             *process). The Grandchild executes the command to the left of the pipe character and writes 
             *to the input end of the pipe, and the child executes the command to the command to the 
             *right of the pipe character and reads from the output end of the pipe. If more than one
             *pipe character were provided, then the pipeerror flag will be set and the user will be 
             *alerted of the error.*/ 
            if (laypipe) {
                if (pipeerror) {
                    fprintf(stderr, "%s\n", "Error: Too many pipes provided...");
                    continue;
                }
                if (pipe(filedes) < 0) {
                    perror("Error creating the pipe");
                    exit(errno);
                }
                /*Flush the grandchild's output buffers to get a fresh buffer free of any errors from the
                 *grandchild's parent.*/
                fflush(stdout);
                fflush(stderr);
                /*Child forks a grandchild for vertical piping. The grandchild executes the first set of
                 *arguments of the pipe, and the child executes the second set of arguments of the pipe.*/
                gkpid = fork();
                if (gkpid < 0) {
                    fprintf(stderr, "%s\n", "Unable to fork grandchild...");
                    continue;
                }
                /*A PID not equal to 0 indicates that this is not the grandchild. This child should execute
                 *the second set of commands of the pipe. The child should Read from the output end of the
                 *pipe and the grandchild should execute the first set of commands and write to the input 
                 *end of the pipe. Per usual, the duplicate file descriptors should be closed.*/
                else if (gkpid != 0) {
                    if (dup2(filedes[0], STDIN_FILENO) < 0) {
                        perror("Error copying file descriptor");
                        exit(errno);
                    }
                    if (close(filedes[0]) < 0) {
                        perror("Error closing old file descriptor");
                        exit(errno);
                    }
                    if (close(filedes[1]) < 0) {
                        perror("Error closing old file descriptor");
                        exit(errno);
                    }
                    /*Have the child execute the supplied command at newargv[childcommandindex]
                     *and offsetting the newargv pointer by the number of newargv slots needed
                     *to reach the second set of commands for the child. The grandchild will execute
                     *the first set of commands in the pipe.*/
                    if ((execvp(newargv[childcommandindex], newargv + childcommandindex)) < 0) {
                        perror("Error executing command");
                        exit(errno);
                    }
                }
                /*A PID of 0 indicates that this is the grandchild of the parental p2 process. The 
                 *grandchild should execute the first set of commands of the pipe. The grandchild 
                 *should write to the input end of the pipe and the child should read from the output
                 *end of the pipe. Per usual, the duplicate file descriptors should be closed.*/
                else {
                    if (dup2(filedes[1], STDOUT_FILENO) < 0) {
                        perror("Error copying file descriptor");
                        exit(errno);
                    }
                    if (close(filedes[0]) < 0) {
                        perror("Error closing old file descriptor");
                        exit(errno);
                    }
                    if (close(filedes[1]) < 0) {
                        perror("Error closing old file descriptor");
                        exit(errno);
                    }
                    /*Have the grandchild execute the supplied command in newargv[0] and check
                     *for errors. The child will execute the second set of commands for the pipe.*/
                    if ((execvp(newargv[0], newargv)) < 0) {
                        perror("Error executing command");
                        exit(errno);
                    }
                }                   
            }
            /*Have the child execute the supplied command in newargv[0] and check for errors.*/
            if ((execvp(newargv[0], newargv)) < 0) {
                perror("Error executing command");
                exit(errno);
            }
        }

        /*Update the previous command. The state of the last execution is saved in a struct array  
         *history. The argv used for the last command is copied into the struct's own argv array and
         *the input/output file name is saved, along with all the flags used including the indicators
         *for input redirect, output redirect, output and error redirect, ambiguous commands, and 
         *whether the command should be a background job or not.*/
        update: 
        history[commandcount - 2].oldargv[0] = strcpy(history[commandcount - 2].oldbuffer, newargv[0]);
        length = (int) strlen(history[commandcount - 2].oldargv[0]) + 1; /*cast strlen to an int to calm splint down.*/
        for(i = 1; i < newargc; i++) {
            if (i == childcommandindex - 1) { 
                history[commandcount - 2].oldargv[i] = NULL;
                length += 1;
            }
            else {
                history[commandcount - 2].oldargv[i] = strcpy(history[commandcount - 2].oldbuffer + length + 1, newargv[i]);
                length += strlen(history[commandcount - 2].oldargv[i]) + 1;
            }
        }
        /*To avoid referencing a NULL value, the file names are checked to see if they are set, if so
         *then the old file name needs to be assigned to the previous command. Otherwise provide the 
         *previous command with the NULL value for the file name.*/
        if (outfile != NULL) {
            history[commandcount - 2].oldoutfile = strcpy(history[commandcount - 2].oldbuffer + length + 1, outfile);
        }
        else history[commandcount - 2].oldoutfile = NULL;
        if (infile != NULL) {
            history[commandcount - 2].oldinfile = strcpy(history[commandcount - 2].oldbuffer + length + 1, infile);
        }
        else history[commandcount - 2].oldinfile = NULL;
        history[commandcount - 2].oldargc = newargc;
        history[commandcount - 2].oldchildcommandindex = childcommandindex;
        history[commandcount - 2].oldinputset = inputset;
        history[commandcount - 2].oldoutputset = outputset;
        history[commandcount - 2].oldoutputerrorset = outputerrorset;
        history[commandcount - 2].oldlaypipe = laypipe;
        history[commandcount - 2].oldambiguityfound = ambiguityfound;
        history[commandcount - 2].oldappendtofile = appendtofile;
        history[commandcount - 2].oldpipeerror = pipeerror;
        history[commandcount - 2].oldbackgroundjob = backgroundjob; 
    }
    /*Kill the process group that has been isolated by setting the process group above. Passing it SIGTERM ensures
     *that the provided signal() call catches it with the custom signal handler function signalcatcher. Of course, 
     *check to make sure that the killpg() call did not produce an error.*/
    if (killpg(getpgrp(), SIGTERM) < 0) {
        perror("Error killing process group");
        exit(errno);
    }
  
    /*Indicate to the user that the p2 shell has successfully terminated and gracefully exit the program.
     *However, if p2 is supplied with command line arguments, then the prompt need not be displayed.*/
    if (argv[1] == NULL) printf("%s\n", "p2 terminated.");
    exit(0);
}

/*The ever important parse function. This parses the user supplied input per line and sets the appropriate flags
 *for file redirects, command ambiguity, and fills up the newargv array with arguments used in the execution
 *of the supplied executable.*/
int parse(char *buff) {
    int wordcount = 0;
    int charactercount = 0;
    int c = 0;
    clearnewarg(); /*Clear out the old newargv array for the new goodies.*/

    for(;;) {
        /*Passing buff + charactercount to the getword function ensures that the line of input is parsed 
         *beyond what the programs has already handled.*/
        c = getword(buff + charactercount);
        
        /*If getword returned a -1 assign the wordcount to newargc and return -1. This tells the program to 
         *terminate after executing the supplied arguments to newargv handling premature EOF.*/ 
        if (c == -1) {
            newargc = wordcount;
            return -1;
        }

        /*Handle an empty newline character provided. This essentially ends a line of input prvided by
         *the user.*/ 
        if (c == 0) break;

        /*Because getword() returns the size of the word, but doesn't include the null terminated character
         *1 needs to be added to c to accomodate this.*/
        charactercount += (c + 1);

        /*Check the input buffer for the file redirect metacharcters. If any of the redirect types, ie input,
         *output, or output and error redirects, have already been set, then set the flag for an ambiguous 
         *command supplied. Set the flag that the redirect has been found, and go to the next word on the input
         *buffer and assign it the the appropriate file pointer. Giving the file pointer the buffer plus the
         *character count minue c minus 1 moves the file pointer to the point that has been read thus far,
         *and brings it back to the beginning of the file name. Subtracting 1 is necessary to accomodate the
         *null terminating character.*/
        if (strcmp((buff + charactercount - 2), "<") == 0) {
            if (inputset) ambiguityfound = 1;
            inputset = 1;
            c = getword(buff + charactercount);
            charactercount += (c + 1);
            infile = buff + charactercount - c - 1;
        }
        else if (strcmp((buff + charactercount - 3), "#") == 0) {
            charactercount += 1;
            newargv[wordcount++] = "#";
            if (commentindex == -1) commentindex = wordcount;
        }
        else if (strcmp((buff + charactercount - 2), "|") == 0) {
            if (pipeescape) {
                newargv[wordcount++] = buff + charactercount - 2;
                charactercount += 1;
            }
            else { 
                if (laypipe) pipeerror = 1;
                laypipe = 1;
                charactercount += 1;
                newargv[wordcount++] = NULL;
                childcommandindex = wordcount;
            }
        }
        else if (strcmp((buff + charactercount - 3), "!$") == 0) {
            replace = 1;
            newargv[wordcount++] = "!$";
        }
        else if (strcmp((buff + charactercount - 3), ">>") == 0) {
            if (outputset || outputerrorset) ambiguityfound = 1;
            outputset = 1;
            appendtofile = 1;
            c = getword(buff + charactercount);
            charactercount += (c + 1);
            outfile = buff + charactercount - c - 1;
        }
        else if (strcmp((buff + charactercount - 2), ">") == 0) {
            if (outputset || outputerrorset) ambiguityfound = 1;
            outputset = 1;
            c = getword(buff + charactercount);
            charactercount += (c + 1);
            outfile = buff + charactercount - c - 1;
        }
        else if (strcmp((buff + charactercount - 4), ">>&") == 0) {
            if(outputerrorset || outputset) ambiguityfound = 1;
            outputerrorset = 1;
            appendtofile = 1;
            c = getword(buff + charactercount);
            charactercount += (c + 1);
            outfile = buff + charactercount - c - 1;
        } 
        else if (strcmp((buff + charactercount - 3), ">&") == 0) {
            if(outputerrorset || outputset) ambiguityfound = 1;
            outputerrorset = 1;
            c = getword(buff + charactercount);
            charactercount += (c + 1);
            outfile = buff + charactercount - c - 1;
        }
        /*If a word on the input line is not associated with a file redirect, then it needs to be added to the
         *newargv array for command execution.*/
        else {
            newargv[wordcount++] = (buff + charactercount - c - 1); 
        }
     }
  
    newargc = wordcount;
    return wordcount;
}

/*Helper function to clear out the newargv array for each new line of input provided by the user.*/
void clearnewarg() {
    int index;

    for (index = 0; index < newargc; index++) {
        newargv[index] = NULL;
    }

    newargc = 0;
}

/*Custom signal handler function. Acts to intercept the SIGTERM signal sent by the main process that the p2
 *program runs in.*/
void signalcatch(int signum) {
    ;
}
