/* Gregory Scott Marvin 
 * Program 1 Due 09/16/19
 * CS 570 Fall 2019 Dr. John Carroll
 * San Diego State University
 * Program adapted from L.S. Foster's inout2.c from C by Discovery 
 */

#include "getword.h"

int pipeescape = 0; //Used to signify an escaped pipe character in p2.c

int getword(char *w){
    int inchar; //inchar takes a character from input for operation
    int length = 0; //length is used to keep track of word length

    //Continues the operation of the program until End Of File is reached from standard in or file   
    while ((inchar = getchar()) != EOF){
        //skip leading whitespace
        if (length == 0 && inchar == ' ') continue;

        //if the character is not a space or newline, add it to the word buffer
        if (inchar != ' ' && inchar != '\n') {
            *w++ = inchar;
            length++;
        }

        //stop adding characters to the buffer once 254 is reached and null terminate the string
        if (length == 254) {
            *w = '\0';
            return 254; 
        }

        if (length == 0 && inchar == '\n') {
            *w = '\0';
            return 0;
        }

        /*greedy metacharacter grabber, initially check if the character is > to see if any
        following characters satisfy the metacharacters >>, >&, or >>& */
        if (length == 1 && inchar == '>') {
            inchar = getchar(); //Collect the next character in the input buffer to check for >> or >&
            if (inchar == '>' || inchar == '&') {
                *w++ = inchar;
                length++;
                if (inchar == '&') {
                    *w = '\0'; //Null terminate the string for the metacharacter >&
                    return 2;
                }
                else if (inchar == '>') {
                    inchar = getchar(); //Collect the next character to check for the metacharacter >>&
                    if (inchar == '&') {
                        *w++ = inchar;
                        length++;
                        *w = '\0'; //Null terminate the string for the metacharacter >>&
                        return 3;
                    }
                    else {
                        /*Give the character back to the buffer if the character is not a &, while the
                        the metacharacter grabber is greedy, it shall not keep unnecessary characters*/ 
                        ungetc(inchar, stdin);
                        *w = '\0'; //Null terminate the string for the metacharacter >>
                        return 2;
                    }
                }
            }
            /*Give the character back to the buffer if the character is not a > or a &. the greedy
            metacharacter grabber shall not keep unnecessary characters hostage*/
            else ungetc(inchar, stdin); 
            *w = '\0'; //Null terminate the string for the methacharacter >
            return 1;
        }

        //Collect the metacharcters <, |, #, or &, null terminate the single character string for use as an instruction
        if (length == 1 && (inchar == '<' || inchar == '|' ||
                            inchar == '#' || inchar == '&')) {
            if (inchar == '#') {
                length++;
                continue;
            }
            else {
                *w = '\0';
                return 1;
            }
        }

        /*Check for a newline terminated string. Give the newline back to the input buffer to illicit a new prompt
        on its own fresh line*/
        if (length > 0 && inchar == '\n') {
            *w = '\0';
            ungetc(inchar, stdin);
            //Check for the string done that precedes a newline to quit program execution early
            /*if (length == 4 && strcmp((w - 4), "done") == 0) return -1; 
            else*/ return length;
        }
         
        if (length > 0 && inchar == ' ') {
            *w = '\0';
            //Check for the string done that preceds a space character to quit program execution early
            /*if (length == 4 && strcmp((w - 4), "done") == 0) return -1;
            else*/ return length;
        }

        /*Check for the metacharacters >, <, |, #, or & to terminate a word in the unput buffer. Null terminate
        the string without the metacharacter, and return the metacharacter back to the input buffer for its 
        individual collection*/
        if (length > 0 && (inchar == '>' || inchar == '<' || inchar == '|' ||
                           inchar == '#' || inchar == '&')) {
            *(w - 1) = '\0';
            ungetc(inchar, stdin);
            return length - 1; //length - 1 is returned to account for the metacharacter returned to the buffer
        }

        /*Escape character sequence. checking for inchar == '\\' is necessary to check for the character \, 
         otherwise the ' is escaped. The next character in the input buffer is collected, and if it is not
         a newline character overwrite the \ with the collected character, and then move the input buffer pointer
         past the collected character (2 characters over from the original \ collected). If it is a newline
         character, then Null terminate the string before the newline, and give the newline back to the input
         buffer so a new input prompt can be illicited on its own fresh line*/
        if (inchar == '\\') {
            inchar = getchar();
            if (inchar == '|') pipeescape = 1;
            if (inchar != '\n') {
                *(w - 1) = inchar;
                *(w + 2);
            }
            else {
                *(w - 1) = '\0';
                ungetc(inchar, stdin);
                return length - 1;
            } 
        }
    }
    
    /*When EOF if reached and an input word has not been terminated with any metacharacter, a space character, or
     newline character, Null terminate the word collected and check if that word is "done" and send back a -1
     terminating program execution early, otherwise send back the length of the word. If the program has reached
     EOF after all input words have been collected, then send back a -1 to terminate program execution*/ 
    *w = '\0';
    if (length > 0) {
        if (length == 4 && strcmp((w - 4), "done") == 0) return -1; 
        else return length;
    }
    else return -1;
}
