// CPSC 351-06
// UNIX Shell Project

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_LINE 80     //maximum length command

int find_pipe(char** cargs){
    int index = 0;
    while(cargs[index] != '\0'){
        if(!strncmp(cargs[index], "|", 1)){ // found pipe
            return index;
        }
        index++;
    }
    return -1;
}


char** token_key(char* input, char* cargs[]){
    const char delimeter[2] = {' ', '\t'}; // delimiters (space and tab)
    char* token;
    int num_args = 0;

    token = strtok(input, delimeter);
    cargs[0] = token;

    char** redirect = malloc(2 * sizeof(char*));
    for(int i = 0; i < 2; i++){
        redirect[i] = malloc(MAX_LINE * sizeof(char));
    }
    redirect[0] = ""; // input/output/pipe
    redirect[1] = ""; // path

    while(token != NULL){
        // add token to cargs
        token = strtok(NULL, delimeter);
        if(token == NULL) break;
        if(!strncmp(token, ">", 1)){
            token = strtok(NULL, delimeter);
            redirect[0] = "o";     //output
            redirect[1] = token;
            return redirect;
        } 
        else if(!strncmp(token, "<", 1)){
            token = strtok(NULL, delimeter);
            redirect[0] = "i";     //input
            redirect[1] = token;
            return redirect;
        }
        else if(!strncmp(token, "|", 1)){
            redirect[0] = "p";     //pipe   
        }
        cargs[++num_args] = token;
    }
    return redirect;
}


int main(int argc, const char* argv[]){
    char input[MAX_LINE];
    char recent[MAX_LINE]; 
    int pipefd[2];         // file descriptor
    int running = 1;

    // clear buffers
    memset(input, 0, MAX_LINE * sizeof(char));
    memset(recent, 0, MAX_LINE * sizeof(char));

    while(running){
        printf("osh> ");  // command line prefix
        fflush(stdout);   // force flush to console
        
        fgets(input, MAX_LINE, stdin);   // get inputs
        input[strlen(input) - 1] = '\0'; // replace newline with null
    
        if(strncmp(input, "exit()", 6) == 0) //leave program when "osh> exit()"
            return 0;
        if(strncmp(input, "!!", 2)) // repeat recently used command
            strcpy(recent, input);      

        bool waiting = true;
        char* wait_offset = strstr(input, "&"); //allow shell to run concurrently
        if(wait_offset != NULL){
            *wait_offset = ' ';
            waiting = false;
        }

        //fork child process
        pid_t pid = fork();
        if(pid < 0){                             
            fprintf(stderr, "Fork Failed\n"); // < 0 means child process creation is unsuccessful
            return -1; // exit code
        }
        if(pid != 0){  // parent process
            if(waiting){
                wait(NULL); //turn off if used with '&'
                wait(NULL);
            }
        }
        else{                         // child process
            char* cargs[MAX_LINE];    // buffer to hold command line arguments
            memset(cargs, 0, MAX_LINE * sizeof(char));

            int history = 0;
            if(!strncmp(input, "!!", 2)) history = 1;
            char** redirect = token_key((history ? recent : input), cargs);
 
            if(history && recent[0] == '\0'){
                printf("No commands in history.\n"); 
                exit(0);
            } 

            if(!strncmp(redirect[0], "o", 1)){ // redirect output
                printf("Output saved to ./%s\n", redirect[1]);
                int folder = open(redirect[1], O_TRUNC | O_CREAT | O_RDWR);
                dup2(folder, STDOUT_FILENO); 
            }else if(!strncmp(redirect[0], "i", 1)){ // redirect input
                printf("Reading from file: ./%s\n", redirect[1]);
                int folder = open(redirect[1], O_RDONLY); // read-only
                memset(input, 0, MAX_LINE * sizeof(char));
                read(folder, input,  MAX_LINE * sizeof(char));
                memset(cargs, 0, MAX_LINE * sizeof(char));
                input[strlen(input) - 1]  = '\0';
                token_key(input, cargs);
            }else if(!strncmp(redirect[0], "p", 1)){ // pipe found
                pid_t pidc; // hierachical child
                int right_pipe_offset = find_pipe(cargs);
                cargs[right_pipe_offset] = "\0";

                int fail_pipe = pipe(pipefd);
                if(fail_pipe < 0){
                    fprintf(stderr, "Pipe creation failed...\n");
                    return 1;
                }

                char* left[MAX_LINE];
                char* right[MAX_LINE];
                memset(left, 0, MAX_LINE*sizeof(char));
                memset(right, 0, MAX_LINE*sizeof(char));

                for(int i = 0; i < right_pipe_offset; ++i){
                    left[i] = cargs[i];
                }

                for(int i = 0; i < MAX_LINE; ++i){
                    int index = i + right_pipe_offset + 1;
                    if(cargs[index] == 0) break;
                    right[i] = cargs[index];
                }
                
                pidc = fork(); //create child to handle pipes on the right
                if(pidc < 0){
                    fprintf(stderr, "Fork failed...\n");
                    return 1;
                }
                if(pidc != 0){ //parent process 
                    dup2(pipefd[1], STDOUT_FILENO); //duplicate stdout to write end of file descriptor
                    close(pipefd[1]); //close write end of pipe
                    execvp(left[0], left); //execute command in child
                    exit(0); 
                }else{ //child process
                    dup2(pipefd[0], STDIN_FILENO); //copy read end of pipe to stdin
                    close(pipefd[0]); //close read end of pipe
                    execvp(right[0], right); //execute command in child
                    exit(0); 
                }
                wait(NULL);
            }
            execvp(cargs[0], cargs);
            exit(0);  
        }
    }
    return 0;
}
