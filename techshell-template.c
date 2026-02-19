/* Name(s): Charles Walton, Sehat Mahde
* Date: 2/9/2026
* Description: **Include what you were and were not able to handle!**
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <wait.h>


//Functions to implement:
char* CommandPrompt(); // Display current working directory and return user input

struct ShellCommand ParseCommandLine(char* input); // Process the user input (As a shell command)

void ExecuteCommand(struct ShellCommand command); //Execute a shell command

void FreeCommand(struct ShellCommand *cmd);

static void redirect_handler(char **args);

struct ShellCommand {
    char **argv;      // execvp args (argv[0] is command, last must be NULL)
    char *in_file;    // filename after <
    char *out_file;   // filename after >
};

int main() {
    char* input;
    struct ShellCommand command;

    // repeatedly prompt the user for input
    for (;;)
    {
        input = CommandPrompt();

        if (input[0] == '\0') {   // user just hit Enter
            free(input);
            continue;
        }
	
        // parse the command line
        command = ParseCommandLine(input);
        // execute the command
        ExecuteCommand(command);

        // Freeing the command to avoid memory leakage (segmentation fault)
        FreeCommand(&command);
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

struct ShellCommand ParseCommandLine(char* input) {
    struct ShellCommand cmd;
    cmd.in_file = NULL;
    cmd.out_file = NULL;

    cmd.argv = malloc(sizeof(char*) * 100);
    if (!cmd.argv) {
        perror("malloc");
        cmd.argv = NULL;
        return cmd;
    }

    int argc = 0;

    char *token = strtok(input, " ");
    
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token) cmd.in_file = strdup(token);
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            if (token) cmd.out_file = strdup(token);
        } else {
            cmd.argv[argc++] = strdup(token);
        }
        token = strtok(NULL, " ");
    }

    cmd.argv[argc] = NULL;   // even if argc==0
    return cmd;
}


void ExecuteCommand(struct ShellCommand command) {
    if (command.argv == NULL || command.argv[0] == NULL)
    	return;

    if (strcmp(command.argv[0], "exit") == 0){
        exit(0);
    }

    
    if (strcmp(command.argv[0], "cd") == 0){
        char *dir = command.argv[1];

        if (dir == NULL){
            fprintf(stderr, "cd: missing argument\n");
            return;
        }

        if (chdir(dir) != 0){
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        }
        return;
    
    }
    
    pid_t pid = fork();


    // process couldn't be creatd, hence -ve
    if (pid < 0) {
        fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        return;
    }


    if (pid == 0){
        execvp(command.argv[0], command.argv);

	// only if anything wrong is typed in (e.g. an invalid flag, typos, etc.)
	fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        exit(1);

    	}

    // parent: wait for child to finish
    else {
        int status;
        waitpid(pid, &status, 0);
	}


}



static void redirect_handler(char **args) {
    for (int i = 0; args[i] != NULL; i++ ){ // -> loops through wtv user types
        
        if (strcmp(args[i], "<") == 0) {
             if (args[i + 1] == NULL){
                perror("Filename doesn't exist");
                _exit(1);
            }
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0){
                perror("input"); // -> This checks if theres a valid file
                _exit(1);

            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
            args[i+1] = NULL;
            i += 1;
        }
        
        else if (strcmp(args[i], ">") == 0)  {
            if (args[i + 1] == NULL){
                perror("Filename doesn't exist");
                _exit(1);
            }
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0){
                perror("output error.");
                _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        args[i] = NULL;
        args[i + 1] = NULL;
        i += 1;
        }
            
    }

}


// Helper to free cmd after every execution
void FreeCommand(struct ShellCommand *cmd) {
    if (!cmd) return;

    if (cmd->argv) {
        for (int i = 0; cmd->argv[i] != NULL; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }

    free(cmd->in_file);
    free(cmd->out_file);

    cmd->argv = NULL;
    cmd->in_file = NULL;
    cmd->out_file = NULL;
}

