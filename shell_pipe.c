/* Name(s): Charles Walton, Sehat Mahde
* Date: 2/9/2026
* Description: **Include what you were and were not able to handle!**
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

struct ShellCommand *ParseCommandLine(char* input, int *ncmds); // Process the user input (As a shell command)

void ExecuteCommand(struct ShellCommand *command, int ncmds); //Execute a shell command

void FreeCommand(struct ShellCommand *cmd, int ncmds);

static void redirection(struct ShellCommand command);

struct ShellCommand {
    // char ***argv;    // array of commands(e.g. **argv[0] = ls -l , **argv[1] = wc -l 
    char **argv;      // execvp args (argv[0] is command, last must be NULL)
    char *in_file;    // filename after <
    char *out_file;   // filename after >
};

int main() {
    char* input;
    struct ShellCommand *command = NULL;
    int ncmds = 0;

    // repeatedly prompt the user for input
    for (;;)
    {
        input = CommandPrompt();

        if (input[0] == '\0') {   // user just hit Enter
            free(input);
            continue;
        }
        // parse the command line
        command = ParseCommandLine(input, &ncmds);
        // execute the command
        ExecuteCommand(command, ncmds);

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

    // Strip trailing newline
    line[strcspn(line, "\n")] = '\0';

    // Return heap copy so caller can keep it
    return strdup(line);
}

struct ShellCommand *ParseCommandLine(char* input, int *ncmds) {
    struct ShellCommand *cmd = calloc(10, sizeof(*cmd));

    if (cmd == NULL) {
        perror("calloc");
        return NULL;
    }

    cmd[0].in_file = NULL;
    cmd[0].out_file = NULL;
    cmd[0].argv = malloc(sizeof(char*) * 100);     // array of 100 array 
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

        else cmd[count_command].argv[argc++] = strdup(token); // standard command; add into argv array 
        token = strtok(NULL, " ");	// call the tokenizer

    }
    cmd[count_command].argv[argc] = NULL;
    *ncmds = count_command+1;
    return cmd;
}


void ExecuteCommand(struct ShellCommand *command, int ncmds) {
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
        // parent: wait for child to finish
        else 
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

    
    // inside the pipe
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

    // waiting for all the children
    for (int i = 0; i < ncmds; i++)
        waitpid(pids[i], NULL, 0);
            
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


