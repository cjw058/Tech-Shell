This program implements a simplified Unix-like shell that supports command execution, piping, input and output redirection, and background processing. 
  The shell continuously runs in an infinite loop, 
  prompting the user for input, parsing that input into structured commands, executing commands, 
  then freeing any allocated memory before repeating. 
The main control flow is handled within the main() function, which repeatedly performs several key tasks. 
  First, it uses waitpid(-1, &status, WNOHANG) in a loop to clean up any terminated background processes, preventing zombie processes from accumulating. 
  It reads user input using a prompt function and ignores empty inputs. 
  Valid input is passed to the parsing function, which constructs an internal representation of the command. 
  The parsed commands are then executed, all memory is free.

The ParseCommandLine() function is responsible for breaking down the user’s input into usable components. 
  It tokenizes the input string using strtok() and identifies special symbols for redirection, pipelines, and background execution 
  Each segment of a pipeline is stored as a separate ShellCommand structure, with arguments put into argv array that is NULL-terminated for compatibility with execvp().
  The function also tracks the number of commands in the pipeline and whether the command should run in the background.

Execution is handled by the ExecuteCommand() function, which manages process creation and communication between processes. 
  It first creates the necessary pipes based on the number of commands, then forks a child process for each command. 
  Depending on the position of the command in the pipeline, each child process redirects its standard input and/or output using dup2() to connect to the appropriate pipe ends. 
  If input or output redirection is specified, it is applied before execution. 
  Each child process then calls execvp() to replace itself with the desired program. 
  Meanwhile, the parent process closes all unused pipe file descriptors to ensure proper cleanup and correct pipe behavior. 
  If the command is running in the foreground, the parent waits for all child processes to finish; otherwise, it immediately returns control to the user.

The FreeCommand() helps with memory storage. 
It frees each argument string within the argv arrays, the arrays themselves, any strings used for input or output redirection, and finally the array of ShellCommand structures. 
This prevents memory leaks and maintains functionality through executions.

