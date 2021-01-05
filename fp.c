#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>

#define CMD_HISTORY_SIZE 10 // the size is 10 for last 10 orders.

static char * cmd_history[CMD_HISTORY_SIZE];
static int cmd_history_count = 0;

// function to receive and process the command from the user

static void exec_cmd(const char * line)
{
	char * CMD = strdup(line); // get the cmd
	char *params[10];
	int argc = 0;

	// parse command parameters

	params[argc++] = strtok(CMD, " ");  // spertors for the prameter by spacing
	while(params[argc-1] != NULL){	// as long as it buys tokens 
		params[argc++] = strtok(NULL, " "); 
	}

	argc--; // Decrement argc by one so that it does not take the null parameter as a token.

	// background running controls

	int background = 0;
	if(strcmp(params[argc-1], "&") == 0){  
		background = 1; // set to run in the background
		params[--argc] = NULL;	// params arrayinden "&" yi sil
	}

	int fd[2] = {-1, -1};	// input and output for fd

	while(argc >= 3){

		// routing parameter control

		if(strcmp(params[argc-2], ">") == 0){	// output

			// open file
			 

	fd[1] = open(params[argc-1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP|S_IWGRP);
			if(fd[1] == -1){
				perror("open");
				free(CMD);
				return;
			}

			// update parameter array

			params[argc-2] = NULL;
			argc -= 2;
		}else if(strcmp(params[argc-2], "<") == 0){ // input
			fd[0] = open(params[argc-1], O_RDONLY);
			if(fd[0] == -1){
				perror("open");
				free(CMD);
				return;
			}
			params[argc-2] = NULL;
			argc -= 2;
		}else{
			break;
		}
	}

	int status;
	pid_t pid = fork();	// create a new transaction
	switch(pid){
		case -1:	 
			perror("fork");
			break;
		case 0:	// child
			if(fd[0] != -1){	 
				if(dup2(fd[0], STDIN_FILENO) != STDIN_FILENO){
					perror("dup2");
					exit(1);
				}
			}
			if(fd[1] != -1){	 
				if(dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO){
					perror("dup2");
					exit(1);
				}
			}
			execvp(params[0], params);
			perror("execvp");
			exit(0);
		default: // parent
			close(fd[0]);close(fd[1]);
			if(!background)
				waitpid(pid, &status, 0);
			break;
	}
	free(CMD);
}

// Function to add commands to history

static void add_to_history(const char * cmd){
	if(cmd_history_count == (CMD_HISTORY_SIZE-1)){  //if history is full 
		int i;
		free(cmd_history[0]);	// release the first command in history from address
		
		// shift other commands to an index

		for(i=1; i < cmd_history_count; i++)
			cmd_history[i-1] = cmd_history[i];
		cmd_history_count--;
	}
	cmd_history[cmd_history_count++] = strdup(cmd);	// add command to history
}

// Function to run commands from history

static void run_from_history(const char * cmd){
	int index = 0;
	if(cmd_history_count == 0){
		printf("No commands in history\n");
		return ;
	}
	
// the second character is '!' get the index of the last command entered
	if(cmd[1] == '!') 
		index = cmd_history_count-1;
	// get the second character for the history index from the user
	else{
		index = atoi(&cmd[1]) - 1; 
		if((index < 0) || (index > cmd_history_count)){ // Print error if there is no such index in history
			fprintf(stderr, "No such command in history.\n Error execution command");
			return;
		}
	}
	printf("%s\n", cmd);	// print what index command was entered  ???
	add_to_history(cmd_history[index]);
	
	exec_cmd(cmd_history[index]);	// run the command at that index
}

// Function to print items in the history buffer

static void list_history(){
	int i;
	for(i=cmd_history_count-1; i >=0 ; i--){	// for each element of history
		printf("%i %s\n", i+1, cmd_history[i]);
	}
}

// function for signal processing

static void signal_handler(const int rc){
	switch(rc){
		case SIGTERM:
		case SIGINT:
			break;
		
		case SIGCHLD:
			
	/* wait for all dead operations and if a child ends in another part of the program
Use non-blocking calls so that signal hands do not block operation */
			
			while (waitpid(-1, NULL, WNOHANG) > 0);
			break;
	}
}

// main

int main(int argc, char *argv[]){

	// catch the signals

	struct sigaction act, act_old;
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	if(	(sigaction(SIGINT,  &act, &act_old) == -1) ||
		(sigaction(SIGCHLD,  &act, &act_old) == -1)){ // Ctrl^C
		perror("signal");
		return 1;
	}

	// allocate buffer for line

	size_t line_size = 100;
	char * line = (char*) malloc(sizeof(char)*line_size);
	if(line == NULL){	// false malloc see yourself until you see
		perror("malloc");
		return 1;	
	}

	int inter = 0; // flag for line retrieval
	while(1){
		if(!inter)
			printf("mysh > ");
		if(getline(&line, &line_size, stdin) == -1){	// read lines to get input
			if(errno == EINTR){	  
				clearerr(stdin);  
				inter = 1;	 
				continue;  
			}
			perror("getline");
			break;
		}
		inter = 0;  

		int line_len = strlen(line);
		if(line_len == 1){	 
			continue;
		}
		line[line_len-1] = '\0';  

		
		if(strcmp(line, "exit") == 0){  
			break;
		}else if(strcmp(line, "history") == 0){  
			list_history();
		}else if(line[0] == '!'){  
			run_from_history(line);
		}else{  
			add_to_history(line);  
			exec_cmd(line);  
		}
	}

	free(line);
	return 0;
}
