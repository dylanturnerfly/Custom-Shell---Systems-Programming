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
void builtin_cd(char** args);
void builtin_pwd(char** args);
void builtin_which(char** args);
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
        printf("mysh> ");
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
    int fd = open(scriptPath, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int lastExitStatus = 0;

    while ((bytes_read = read(fd, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        lastExitStatus = executeCommand(buffer, lastExitStatus);
    }

    close(fd);
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

// This implementation does not handle error cases where redirection operators are used incorrectly (e.g., missing file names, multiple redirections without proper handling). You may need to add additional logic to handle these cases appropriately.
// Also, this function does not handle appending redirection >>. If needed, you can add similar logic to detect and handle it.
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
    char *paths[] = {"/usr/local/bin/", "/usr/bin/", "/bin/"};
    struct stat statbuf;

    if (strchr(command, '/') != NULL) {
        // Command already has a path
        strcpy(fullPath, command);
        return;
    }

    for (int i = 0; i < 3; ++i) {
        strcpy(fullPath, paths[i]);
        strcat(fullPath, command);
        if (access(fullPath, X_OK) == 0 && stat(fullPath, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            // Executable file found
            return;
        }
    }

    // Command not found in the specified directories
    fullPath[0] = '\0';
}

void builtin_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mysh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("mysh");
        }
    }
}

void builtin_pwd(char **args) {
    char cwd[BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("mysh");
    }
}

void builtin_which(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mysh: expected argument to \"which\"\n");
    } else {
        char fullPath[BUFFER_SIZE];
        findFullPath(args[1], fullPath);
        if (strlen(fullPath) > 0) {
            printf("%s\n", fullPath);
        } else {
            printf("mysh: %s: command not found\n", args[1]);
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
    int groupCount = 0;
    int i = 0;

    cmdGroups[groupCount] = &args[i];
    groupCount++;

    while (args[i] != NULL) {
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL; // Split the command here
            if (groupCount >= bufferSize) {
                bufferSize += 64;
                cmdGroups = realloc(cmdGroups, bufferSize * sizeof(char**));
            }
            cmdGroups[groupCount] = &args[i + 1];
            groupCount++;
        }
        i++;
    }

    *cmdCount = groupCount;
    return cmdGroups;
}


// Tokenize and execute the command
// Handle built-in commands or fork and exec for external commands
// Manage redirection and pipes
int executeCommand(char* cmd, int lastExitStatus) {
    char** args = parseCommand(cmd);
    int tokenCount = 0;
    while (args[tokenCount] != NULL) tokenCount++;

    if (args[0] == NULL) {
        free(args);
        return 0; // An empty command was entered.
    }

    // Handling conditional commands
    if (strcmp(args[0], "then") == 0) {
        if (lastExitStatus != 0) {
            free(args);
            return lastExitStatus; // Skip if last command failed
        }
        args++; // Move past the 'then'
    } else if (strcmp(args[0], "else") == 0) {
        if (lastExitStatus == 0) {
            free(args);
            return lastExitStatus; // Skip if last command succeeded
        }
        args++; // Move past the 'else'
    }

    // Expand wildcards
    char** expandedArgs = expandWildcards(args, &tokenCount);

    int cmdCount = 0;
    char*** cmdGroups = splitCommandAtPipe(expandedArgs, &cmdCount);

    int status = 0;


// Check for built-in commands
    if (strcmp(args[0], "cd") == 0) {
        builtin_cd(args);
    } else if (strcmp(args[0], "pwd") == 0) {
        builtin_pwd(args);
    } else if (strcmp(args[0], "which") == 0) {
        builtin_which(args);
    } else {
        if (cmdCount == 1) {
            // No pipes, execute command normally
            int pid = fork();

            if (pid == 0) {
                // Child process
                if (isRedirection(expandedArgs)) {
                    setupRedirection(expandedArgs);
                }
                char fullPath[BUFFER_SIZE];
                findFullPath(expandedArgs[0], fullPath);
                if (strlen(fullPath) > 0) {
                    execv(fullPath, expandedArgs);
                }
                perror("execv");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                // Parent process
                waitpid(pid, &status, 0); // Wait for child process to finish
            } else {
                perror("fork");
            }
        } else {
            // Handle pipeline
            int fds[2];
            if (pipe(fds) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < cmdCount; i++) {
                int pid = fork();
                if (pid == -1) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }

                if (pid == 0) {
                    if (i == 0) { // First command
                        close(fds[0]);
                        dup2(fds[1], STDOUT_FILENO);
                    } else { // Second command
                        close(fds[1]);
                        dup2(fds[0], STDIN_FILENO);
                    }

                    // Execute the command
                    char fullPath[BUFFER_SIZE];
                    findFullPath(cmdGroups[i][0], fullPath);
                    if (strlen(fullPath) > 0) {
                        execv(fullPath, cmdGroups[i]);
                    }
                    perror("execv");
                    exit(EXIT_FAILURE);
                }
            }

            close(fds[0]);
            close(fds[1]);

            for (int i = 0; i < cmdCount; i++) {
                wait(NULL); // Wait for all child processes
            }
        }
    }
    free(args);
    free(expandedArgs);
    free(cmdGroups);

    return WEXITSTATUS(status);
}