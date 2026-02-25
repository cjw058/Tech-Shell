/* Name(s): Charles Walton, Sehat Mahde
* Date: 2/9/2026
* Description: This C program implements a simple shell that can execute commands, handle input/output redirection, 
               and support piping between commands. 
               It also includes a command prompt that displays the current working directory and allows users to enter commands. 
               It parses the user input to identify commands, arguments, redirection operators, and background execution flags,
               and then executes the commands accordingly using fork and execvp.
               It handles input redirection using '<' and output redirection using '>', allowing users to specify files for reading and writing.
               It also manages multiple commands connected by pipes, setting up the necessary file descriptors for inter-process communication.
               (e.g., "ls -l | grep .c  > output.txt | sleep 3 &" will list files, filter for .c files, and write the output to output.txt).
               The program also handles background execution of commands using '&' and ensures proper cleanup of child processes to prevent zombies. 
               Certain error conditions are handled gracefully, with appropriate error messages printed to stderr.
               
               Limitations: The shell does not support advanced features like command history, job control, or complex quoting/escaping of arguments. 
                            It also assumes 1000 commands in a pipeline and 10000 arguments per command, which may not be sufficient for all use cases.
                            Also does not handle multiple redirections in a single command (e.g., "ls > out1.txt > out2.txt" will only redirect to out2.txt).
                            It also does not handle cases where the user might input invalid syntax (e.g., "ls >" or "cat <") and will simply print an error message without executing any command.
                            Besides, it does not handle signals (e.g., Ctrl+C) to terminate running processes, and it does not implement any built-in commands other than 'cd' and 'exit'.
                            
                Overall, this program serves as a basic implementation of a shell with limited support for command execution, redirection, and piping, while also demonstrating error handling and process management in C.
                            */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>   
#include <fcntl.h>
#include <errno.h>
#include <limits.h>


#define READ_END 0
#define WRITE_END 1 

//Functions to implement:
char* CommandPrompt(); // Display current working directory and return user input

struct ShellCommand *ParseCommandLine(char* input, int *ncmds, int *background); // Process the user input (As a shell command)

void ExecuteCommand(struct ShellCommand *command, int ncmds, int background); //Execute a shell command

void FreeCommand(struct ShellCommand *cmd, int ncmds);

static void redirection(struct ShellCommand command);

struct ShellCommand {
    char **argv;      // execvp args (argv[0] is command, last must be NULL)
    char *in_file;    // filename after <
    char *out_file;   // filename after >
};

int main() {

    // repeatedly prompt the user for input
    for (;;){   
        char* input;
        struct ShellCommand *command = NULL;
        int ncmds = 0;
        int background = 0;
        int status;

        // Adding “reaping” to prevent zombies
        pid_t st;
        while ((st = waitpid(-1, &status, WNOHANG) > 0)){
            if (st == -1 && errno != ECHILD) {
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
         }
        }      // -1 = any child, WNOHANG = "don't stop"


        input = CommandPrompt();

        if (input[0] == '\0') {   // when hit Enter
            free(input);
            continue;
        }
        // parsing the command line
        command = ParseCommandLine(input, &ncmds, &background);
        // executing the command
        ExecuteCommand(command, ncmds, background);

        // Freeing the command to avoid memory leakage (segmentation fault)
        FreeCommand(command, ncmds);
        free(input);
    }
    exit(0);

}

char* CommandPrompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        strcpy(cwd, ""); // fallback prompt
    }

    printf("%s$ ", cwd);
    fflush(stdout);

    char line[4096];
    if (fgets(line, sizeof(line), stdin) == NULL) {
        // EOF (Ctrl+D) or input error -> exit gracefully
        printf("\n");
        exit(0);
    }

    // Striping trailing newline
    line[strcspn(line, "\n")] = '\0';

    // Returning heap copy so caller can keep it
    return strdup(line);
}

struct ShellCommand *ParseCommandLine(char* input, int *ncmds, int *background) {
    // Allocate memory for the command structure to 1000 is reached, which is the maximum input size for a command line.
    struct ShellCommand *cmd = calloc(1000, sizeof(struct ShellCommand)); // array of memory buffer size ShellCommand

    if (cmd == NULL) {
        perror("calloc");
        return NULL;
    }

    cmd[0].in_file = NULL;
    cmd[0].out_file = NULL;
    cmd[0].argv = malloc(sizeof(char*) * 10000);     // array of 10000 array 
    if (!cmd[0].argv) {
        perror("malloc");
        free(cmd);
        return NULL;
    }

    int count_command = 0;
    int argc = 0;
    char *token = strtok(input, " ");

    while (token != NULL) {

        // if token is an infile
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) { // missing filename
                fprintf(stderr, "Error %d (%s)\n", EINVAL, strerror(EINVAL));
                // cleanup minimal
                *ncmds = 0;
                return cmd;
            }

            cmd[count_command].in_file = strdup(token);
        } 

        // if token is an outfile
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) { // missing filename
                fprintf(stderr, "Error %d (%s)\n", EINVAL, strerror(EINVAL));
                *ncmds = 0;
                return cmd;
            }

            cmd[count_command].out_file = strdup(token);
        }

        // if token is 'pipe'
        else if(strcmp(token, "|") == 0){
            cmd[count_command].argv[argc] = NULL;   // even if argc==0
            argc = 0;
            count_command++;

            cmd[count_command].in_file = NULL;
            cmd[count_command].out_file = NULL;
            cmd[count_command].argv = malloc(sizeof(char *) * 100);
            if (cmd[count_command].argv == NULL) {
                perror("malloc");
                *ncmds = count_command; 
                return cmd;
            }
        }

        else if (strcmp(token, "&") == 0){
            if (strtok(NULL, " ") == NULL){
                *background = 1;
                break;
            }

            // perror("DEBUG: '&' isn't getting read");
            fprintf(stderr, "Error %d (%s)\n", EINVAL, strerror(EINVAL));
            *ncmds = 0;
            return cmd;
        }

        else cmd[count_command].argv[argc++] = strdup(token); // standard command; add into argv array 
        token = strtok(NULL, " ");	// call the tokenizer

    }
    cmd[count_command].argv[argc] = NULL;
    *ncmds = count_command+1;

    return cmd;
}


void ExecuteCommand(struct ShellCommand *command, int ncmds, int background) {
    if (command[0].argv == NULL || command[0].argv[0] == NULL)
    	return;

    // if typed 'exit'
    if (strcmp(command[0].argv[0], "exit") == 0){
        exit(0);
    }

    // if typed 'cd'
    if (strcmp(command[0].argv[0], "cd") == 0){
        char *dir = command[0].argv[1];

        if (dir == NULL){
            fprintf(stderr, "cd: missing argument\n");
            return;
        }

        if (chdir(dir) != 0){
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        }
        return;
    
    }

    // no pipe
    if (ncmds == 1){

        pid_t pid = fork();
        // process couldn't be creatd, hence -ve
        if (pid < 0) {
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
            return;
        }
        if (pid == 0){
            if (command[0].in_file || command[0].out_file) redirection (command[0]);

            execvp(command[0].argv[0], command[0].argv);

            // only if anything wrong is typed in (e.g. an invalid flag, typos, etc.)
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                exit(1);
        }

        if (background) {
            fprintf(stdout, "[bg]\t%d\n", pid);
            return;
        }

        // parent: wait for child to finish
        if (!background) 
            waitpid(pid, NULL, 0);
        
        
        return;
    }

    // pipe 

    // creating pipe
    int pipefd[ncmds-1][2]; // e.g. 4 cmd = 3 pipes (0 to 2) 
    int pids[ncmds];
    for(int i = 0; i < ncmds-1; i++){
        if(pipe(pipefd[i]) == -1){
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
            return;
        }
    }

    // implementing pipe
    for(int i = 0; i < ncmds; i++){
        pid_t pid = fork();
        // process couldn't be creatd, hence -ve
        if (pid < 0) {
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
            return;
        }
        if (pid == 0){
            
            if (i > 0) dup2(pipefd[i-1][READ_END], STDIN_FILENO);
            if (i < ncmds-1) dup2(pipefd[i][WRITE_END], STDOUT_FILENO);
           
            for (int k = 0; k < ncmds - 1; k++) {
                close(pipefd[k][READ_END]);
                close(pipefd[k][WRITE_END]);
            }

            if (command[i].in_file || command[i].out_file) redirection (command[i]);
            execvp(command[i].argv[0], command[i].argv);
            
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                exit(1);
        }

        pids[i] = pid;
    }
    
    // closing ends in parent process as well
    for (int k = 0; k < ncmds - 1; k++) {
        close(pipefd[k][READ_END]);
        close(pipefd[k][WRITE_END]);
    }
    

    // showing last process id triggered in background if background execution is specified
    if (background) {
        for(int i = 0; i < ncmds; i++)
            fprintf(stdout, "[%d]\t%d\n", i+1, pids[i]);
        return;
    }

    // waiting for all the children
    if (!background){
        for (int i = 0; i < ncmds; i++)
        waitpid(pids[i], NULL, 0);
    }

            
}

static void redirection(struct ShellCommand command){
		
        // '<'
		if (command.in_file){
		    int file = open(command.in_file, O_RDONLY);	// open in read-only mode
		    if (file < 0){
		        fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
			_exit(1);
		    }
		    dup2(file, STDIN_FILENO);
		    close(file);
		}

        // '>'
		if (command.out_file){
		    int file = open(command.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // open for writing, create if missing, truncate if it exists, permissions rw-r--r--
		    if(file < 0){
		    fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
		    _exit(1);
		    }
		    dup2(file, STDOUT_FILENO);
		    close(file);
		}	
	
}

// Helper to free cmd after every execution
void FreeCommand(struct ShellCommand *cmd, int ncmds) {
    if (!cmd) return;

    int j = 0;
    while(j < ncmds){

        if (cmd[j].argv) {
            for (int i = 0; cmd[j].argv[i] != NULL; i++) {
                free(cmd[j].argv[i]);
            }
            free(cmd[j].argv);
        }

        // free redirection filenames
        free(cmd[j].in_file);
        free(cmd[j].out_file);

        j++;
    }

    free(cmd);
    
}



