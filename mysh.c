#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>

#define BUFFER_SIZE 1024

void batchMode(char* scriptPath);
void interactiveMode();
char** parseCommand(char* cmd);
int isRedirection(char** tokens);
void setupRedirection(char** tokens);
void findFullPath(char* command, char* fullPath);
int builtin_cd(char** args);
int builtin_pwd(char** args);
int builtin_which(char** args);
char** expandWildcards(char** tokens, int* tokenCount);
char*** splitCommandAtPipe(char** args, int* cmdCount);
int executeCommand(char* cmd, int lastExitStatus);


int main(int argc, char *argv[]) {
    if (argc == 2) {
        // Batch mode
        batchMode(argv[1]);
    } else if (argc == 1) {
        // Interactive mode
        interactiveMode();
    }
    return 0;
}

void interactiveMode() {
    char input[BUFFER_SIZE];
    int lastExitStatus = 0;

    printf("Welcome to my shell!\n");
    while (1) {
        printf("\nmysh> ");
        fflush(stdout);

        // Clear the input buffer
        memset(input, 0, BUFFER_SIZE);

        // Read input
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            if (feof(stdin)) {
                break; // End of file (Ctrl+D)
            } else {
                perror("fgets");
                continue; // Error, but continue shell
            }
        }

        // Remove newline character if present
        input[strcspn(input, "\n")] = '\0';

        lastExitStatus = executeCommand(input, lastExitStatus);
    }
    printf("mysh: exiting\n");
}

void batchMode(char* scriptPath) {
    FILE *file = fopen(scriptPath, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    int lastExitStatus = 0;

    while (fgets(line, BUFFER_SIZE, file) != NULL) {
        // Remove newline character if present
        line[strcspn(line, "\n")] = '\0';
        
        // Execute each command
        lastExitStatus = executeCommand(line, lastExitStatus);
    }

    fclose(file);
}



char** parseCommand(char* cmd) {
    int bufferSize = 64;
    int position = 0;
    char** tokens = malloc(bufferSize * sizeof(char*));
    char* token;

    if (!tokens) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Tokenize the input string
    token = strtok(cmd, " \t\r\n\a");
    while (token != NULL) {
        tokens[position] = token;
        position++;

        // If we have exceeded the buffer, reallocate.
        if (position >= bufferSize) {
            bufferSize += 64;
            tokens = realloc(tokens, bufferSize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, " \t\r\n\a");
    }
    tokens[position] = NULL; // Null-terminate the array
    return tokens;
}


int isRedirection(char** tokens) {
    int i = 0;
    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
            return 1; // True
        }
        i++;
    }
    return 0; // False
}

void setupRedirection(char** tokens) {
    int i = 0;

    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], ">") == 0) {
            // Output redirection
            tokens[i] = NULL; // Truncate the command here
            int fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd);
        } else if (strcmp(tokens[i], "<") == 0) {
            // Input redirection
            tokens[i] = NULL; // Truncate the command here
            int fd = open(tokens[i + 1], O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd);
        }
        i++;
    }
}

void findFullPath(char *command, char *fullPath) {
    struct stat statbuf;

    // Check if command starts with a slash (absolute path) or contains a slash (relative path)
    if (strchr(command, '/') != NULL) {
        if (access(command, X_OK) == 0 && stat(command, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            strcpy(fullPath, command);
            return;
        } else {
            fullPath[0] = '\0'; // Command not found or not executable
            return;
        }
    }

    // If no slash is found, search in the specified directories
    char *searchPaths[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    int numPaths = 3;

    for (int i = 0; i < numPaths; i++) {
        snprintf(fullPath, BUFFER_SIZE, "%s/%s", searchPaths[i], command);

        if (access(fullPath, X_OK) == 0 && stat(fullPath, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            return; // Executable file found
        }
    }

    fullPath[0] = '\0'; // Command not found
}









int builtin_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument to \"cd\"\n");
        return 1; // Return failure status
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
            return 1; // Return failure status
        }
        return 0; // Return success status
    }
}


int builtin_pwd(char **args) {
    char cwd[BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0; // Return success status
    } else {
        perror("pwd");
        return 1; // Return failure status
    }
}


int builtin_which(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "which: expected argument to \"which\"\n");
        return 1; // Return failure status
    } else {
        char fullPath[BUFFER_SIZE];
        findFullPath(args[1], fullPath);
        if (strlen(fullPath) > 0) {
            printf("%s\n", fullPath);
            return 0; // Return success status
        } else {
            printf("which: %s: command not found\n", args[1]);
            return 1; // Return failure status
        }
    }
}


char** expandWildcards(char **tokens, int *tokenCount) {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (!d) {
        perror("opendir");
        return tokens;
    }

    int bufferSize = *tokenCount;
    char **expandedTokens = malloc(bufferSize * sizeof(char*));
    if (!expandedTokens) {
        perror("malloc");
        return tokens;
    }

    int newCount = 0;
    for (int i = 0; i < *tokenCount; i++) {
        if (strchr(tokens[i], '*') != NULL) {
            // Found a wildcard, expand it
            while ((dir = readdir(d)) != NULL) {
                if (fnmatch(tokens[i], dir->d_name, 0) == 0) {
                    // Match found, add to expandedTokens
                    if (newCount >= bufferSize) {
                        bufferSize += 64;
                        expandedTokens = realloc(expandedTokens, bufferSize * sizeof(char*));
                        if (!expandedTokens) {
                            perror("realloc");
                            return tokens;
                        }
                    }
                    expandedTokens[newCount++] = strdup(dir->d_name);
                }
            }
            rewinddir(d);
        } else {
            // No wildcard, copy token as is
            if (newCount >= bufferSize) {
                bufferSize += 64;
                expandedTokens = realloc(expandedTokens, bufferSize * sizeof(char*));
                if (!expandedTokens) {
                    perror("realloc");
                    return tokens;
                }
            }
            expandedTokens[newCount++] = strdup(tokens[i]);
        }
    }
    closedir(d);
    expandedTokens[newCount] = NULL;

    // Update the token count
    *tokenCount = newCount;

    return expandedTokens;
}

char*** splitCommandAtPipe(char **args, int *cmdCount) {
    int bufferSize = 64;
    char ***cmdGroups = malloc(bufferSize * sizeof(char**));
    if (!cmdGroups) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int groupCount = 0;
    int start = 0;  // Start index of a command group
    int argsLength = 0; // Calculate the length of args

    // First, find the length of args
    while (args[argsLength] != NULL) {
        argsLength++;
    }

    for (int i = 0; i < argsLength; i++) {
        if (strcmp(args[i], "|") == 0) {
            if (i == start) {  // Empty command before the pipe
                fprintf(stderr, "Syntax error: empty command before pipe\n");
                free(cmdGroups);
                *cmdCount = 0;
                return NULL;
            }

            args[i] = NULL;  // Null-terminate the current command group
            cmdGroups[groupCount++] = &args[start];

            start = i + 1;  // Start index for the next command group

            if (groupCount >= bufferSize) {
                bufferSize += 64;
                cmdGroups = realloc(cmdGroups, bufferSize * sizeof(char**));
                if (!cmdGroups) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // Check if there is a command after the last pipe
    if (start < argsLength) {
        if (args[start] == NULL) {  // Empty command after the last pipe
            fprintf(stderr, "Syntax error: empty command after pipe\n");
            free(cmdGroups);
            *cmdCount = 0;
            return NULL;
        }
        cmdGroups[groupCount++] = &args[start];
    }

    *cmdCount = groupCount;
    return cmdGroups;
}




// Function to execute a single command with redirection
int executeSingleCommand(char** args) {
    pid_t pid;
    int status;

    if ((pid = fork()) == 0) {  // Child process
        // Handle redirection
        setupRedirection(args);

        execvp(args[0], args);  // Execute the command
        perror("execvp");       // execvp failed
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");  // Fork failed
        return 1;
    } else {
        waitpid(pid, &status, 0);  // Parent waits for child
        return WEXITSTATUS(status);
    }
}

// Function to execute piped commands
int executePipedCommand(char*** cmdGroups, int cmdCount) {
    int in_fd = 0, fd[2];
    pid_t pid;
    int status = 0;

    for (int i = 0; i < cmdCount; ++i) {
        // Setup pipe for all but the last command
        if (i < cmdCount - 1) {
            if (pipe(fd) < 0) {
                perror("pipe");
                return 1;
            }
        }

        if ((pid = fork()) == 0) {  // Child process
            // Handle redirection
            setupRedirection(cmdGroups[i]);

            if (i != cmdCount - 1) {  // Not the last command
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }

            if (i != 0) {  // Not the first command
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            execvp(cmdGroups[i][0], cmdGroups[i]);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork");
            return 1;
        }

        // Close the write end of the pipe in the parent
        if (i < cmdCount - 1) {
            close(fd[1]);
        }

        // Close the previous input file descriptor
        if (i != 0) {
            close(in_fd);
        }

        in_fd = fd[0];  // Save the read end of the pipe

        waitpid(pid, &status, 0);  // Parent waits for child
    }

    return WEXITSTATUS(status);
}

// Tokenize and execute the command
int executeCommand(char* cmd, int lastExitStatus) {
    char** args = parseCommand(cmd);
    int tokenCount = 0;
    while (args[tokenCount] != NULL) tokenCount++;

    if (args[0] == NULL) {
        free(args);
        return 0; // An empty command was entered.
    }

    // Handling conditional commands 'then' and 'else'
    if (strcmp(args[0], "then") == 0) {
        if (lastExitStatus != 0 || tokenCount == 1) {
            free(args);
            return lastExitStatus;
        }
        memmove(args, args + 1, (tokenCount - 1) * sizeof(char*));
        args[tokenCount - 1] = NULL;
    } else if (strcmp(args[0], "else") == 0) {
        if (lastExitStatus == 0 || tokenCount == 1) {
            free(args);
            return lastExitStatus;
        }
        memmove(args, args + 1, (tokenCount - 1) * sizeof(char*));
        args[tokenCount - 1] = NULL;
    }

    // Built-in commands
    if (strcmp(args[0], "cd") == 0) {
        int status = builtin_cd(args);
        free(args);
        return status;
    } else if (strcmp(args[0], "pwd") == 0) {
        int status = builtin_pwd(args);
        free(args);
        return status;
    } else if (strcmp(args[0], "which") == 0) {
        int status = builtin_which(args);
        free(args);
        return status;
    } else if (strcmp(args[0], "exit") == 0) {
        printf("mysh: exiting\n");
        free(args);
        exit(0);
    }

    // Expand wildcards
    char** expandedArgs = expandWildcards(args, &tokenCount);

    // Check for redirection and pipes
    int cmdCount = 0;
    char*** cmdGroups = splitCommandAtPipe(expandedArgs, &cmdCount);

    int status = 0;

    if (cmdCount == 0) {  // Syntax error detected
        return -1;  // Return an error status
    }

    if (cmdCount == 1) {
        status = executeSingleCommand(expandedArgs);
    } else {
        status = executePipedCommand(cmdGroups, cmdCount);
    }

    free(args);
    free(expandedArgs);
    free(cmdGroups);
    return status;
}