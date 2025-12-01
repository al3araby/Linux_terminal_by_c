#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "executor.h"

// Function to execute a command string: parse into argv, dispatch to builtin or external.
int execute_command(const char *command) {
    if (command == NULL) return -1;

    // Duplicate since strtok will modify
    char *cmd_copy = strdup(command);
    if (cmd_copy == NULL) {
        perror("strdup");
        return -1;
    }

    // Detect logical AND operator (&&)
    const char *and_ptr = strstr(cmd_copy, "&&");
    if (and_ptr != NULL) {
        /* Split by '&&' and execute sequentially */
        char *parts[32];
        int pcount = 0;
        char *saveptr = NULL;
        char *tok = strtok_r(cmd_copy, "&", &saveptr);
        while (tok != NULL && pcount < 32) {
            /* Skip empty tokens from && */
            if (tok[0] == '&') tok++;
            
            /* Trim leading/trailing whitespace */
            while (*tok == ' ' || *tok == '\t') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
                *end = '\0';
                end--;
            }
            
            if (strlen(tok) > 0) {
                parts[pcount++] = strdup(tok);
            }
            tok = strtok_r(NULL, "&", &saveptr);
        }

        /* Execute commands sequentially, stopping on failure */
        int last_status = 0;
        for (int i = 0; i < pcount; i++) {
            last_status = execute_command(parts[i]);
            if (last_status != 0) {
                /* Command failed, stop executing remaining commands */
                break;
            }
        }

        /* Free duplicated strings */
        for (int i = 0; i < pcount; i++) {
            free(parts[i]);
        }
        free(cmd_copy);
        return last_status;
    }

    // Detect pipelines
    if (strchr(cmd_copy, '|') != NULL) {
        /* Split by '|' and build array of command strings */
        char *parts[32];
        int pcount = 0;
        char *saveptr = NULL;
        char *tok = strtok_r(cmd_copy, "|", &saveptr);
        while (tok != NULL && pcount < 32) {
            /* trim leading/trailing whitespace */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) { *end = '\0'; end--; }
            parts[pcount++] = tok;
            tok = strtok_r(NULL, "|", &saveptr);
        }

        /* Create pipes between commands */
        int prev_fd = -1;
        pid_t children[32];
        int child_count = 0;

        for (int pi = 0; pi < pcount; pi++) {
            int pipefd[2];
            if (pi < pcount - 1) {
                if (pipe(pipefd) == -1) {
                    perror("pipe");
                    free(cmd_copy);
                    return -1;
                }
            }

            pid_t cpid = fork();
            if (cpid < 0) {
                perror("fork");
                free(cmd_copy);
                return -1;
            }

            if (cpid == 0) {
                /* Child */
                if (prev_fd != -1) {
                    dup2(prev_fd, STDIN_FILENO);
                    close(prev_fd);
                }
                if (pi < pcount - 1) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                /* Handle simple redirection in this single command (>, <) */
                char *cmdcopy2 = strdup(parts[pi]);
                char *argv[256];
                int ai = 0;
                char *sp = NULL;
                char *tk = strtok_r(cmdcopy2, " ", &sp);
                int infd = -1, outfd = -1;
                while (tk != NULL && ai < 255) {
                    if (strcmp(tk, ">") == 0) {
                        tk = strtok_r(NULL, " ", &sp);
                        if (tk) {
                            outfd = open(tok, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        }
                    } else if (strcmp(tk, "<") == 0) {
                        tk = strtok_r(NULL, " ", &sp);
                        if (tk) {
                            infd = open(tok, O_RDONLY);
                        }
                    } else {
                        argv[ai++] = tk;
                    }
                    tk = strtok_r(NULL, " ", &sp);
                }
                argv[ai] = NULL;

                if (infd != -1) {
                    dup2(infd, STDIN_FILENO);
                    close(infd);
                }
                if (outfd != -1) {
                    dup2(outfd, STDOUT_FILENO);
                    close(outfd);
                }

                if (ai == 0) {
                    _exit(0);
                }
                execvp(argv[0], argv);
                if (errno == ENOENT) {
                    fprintf(stderr, "Command not found: %s\n", argv[0]);
                    _exit(127);
                } else {
                    perror("execvp");
                    _exit(EXIT_FAILURE);
                }
            }

            /* Parent */
            children[child_count++] = cpid;
            if (prev_fd != -1) close(prev_fd);
            if (pi < pcount - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }

        /* Wait for all children */
        int last_status = 0;
        for (int i = 0; i < child_count; i++) {
            int st = 0;
            waitpid(children[i], &st, 0);
            if (WIFEXITED(st)) last_status = WEXITSTATUS(st);
        }

        free(cmd_copy);
        return last_status;
    }

    /* No pipeline: parse into argv-style array */
    const int MAX_ARGS = 256;
    char *args[MAX_ARGS];
    char *token = strtok(cmd_copy, " ");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (i == 0) {
        free(cmd_copy);
        return -1;
    }

    /* Special handling for 'ls' command: inject --color=auto */
    if (strcmp(args[0], "ls") == 0) {
        /* Create new argv with --color=auto injected */
        char *colored_args[MAX_ARGS];
        colored_args[0] = args[0];  /* "ls" */
        colored_args[1] = "--color=auto";
        int j = 1;
        for (int k = 1; args[k] != NULL && j < MAX_ARGS - 1; k++, j++) {
            colored_args[j] = args[k];
        }
        colored_args[j] = NULL;
        int ret = exec_external(colored_args);
        free(cmd_copy);
        return ret;
    }

    /* Check for built-in commands (exec_builtin runs in the parent when needed)
       List of built-in commands that don't need fork/exec */
    const char *builtins[] = {
        "cd", "exit", "about", "help", "clear", "count", "history", NULL
    };
    
    int is_builtin = 0;
    for (int j = 0; builtins[j] != NULL; j++) {
        if (strcmp(args[0], builtins[j]) == 0) {
            is_builtin = 1;
            break;
        }
    }
    
    if (is_builtin) {
        int ret = exec_builtin(args);
        free(cmd_copy);
        return ret;
    }

    /* Handle simple redirection for single external commands (>, <) */
    int infd = -1, outfd = -1;
    for (int k = 0; k < i; k++) {
        if (strcmp(args[k], ">") == 0) {
            if (k + 1 < i) {
                outfd = open(args[k+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                /* remove the redirection tokens from argv */
                for (int s = k; s + 2 <= i; s++) args[s] = args[s+2];
                i -= 2;
                args[i] = NULL;
            }
        } else if (strcmp(args[k], "<") == 0) {
            if (k + 1 < i) {
                infd = open(args[k+1], O_RDONLY);
                for (int s = k; s + 2 <= i; s++) args[s] = args[s+2];
                i -= 2;
                args[i] = NULL;
            }
        }
    }

    if (infd != -1 || outfd != -1) {
        /* record the full command into history */
        add_command_to_history(command);
        /* perform fork/exec here to apply redirection */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (infd != -1) close(infd);
            if (outfd != -1) close(outfd);
            free(cmd_copy);
            return -1;
        } else if (pid == 0) {
            if (infd != -1) {
                dup2(infd, STDIN_FILENO);
                close(infd);
            }
            if (outfd != -1) {
                dup2(outfd, STDOUT_FILENO);
                close(outfd);
            }
            execvp(args[0], args);
            if (errno == ENOENT) {
                fprintf(stderr, "Command not found: %s\n", args[0]);
                _exit(127);
            } else {
                perror("execvp");
                _exit(EXIT_FAILURE);
            }
        } else {
            int status = 0;
            waitpid(pid, &status, 0);
            if (infd != -1) close(infd);
            if (outfd != -1) close(outfd);
            free(cmd_copy);
            if (WIFEXITED(status)) return WEXITSTATUS(status);
            return -1;
        }
    }

    /* For external commands without redirection, call exec_external which handles history tracking */
    int ret = exec_external(args);
    free(cmd_copy);
    return ret;
}