#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "executor.h"

// Function to execute an external command using argv-style args
int exec_external(char **args) {
    if (args == NULL || args[0] == NULL) return -1;

    /* Track the command in history */
    /* Add full command string (join args) to history */
    size_t len = 0;
    for (int i = 0; args[i] != NULL; i++) {
        len += strlen(args[i]) + 1;
    }
    char *cmdstr = malloc(len + 1);
    if (cmdstr) {
        cmdstr[0] = '\0';
        for (int i = 0; args[i] != NULL; i++) {
            if (i) strcat(cmdstr, " ");
            strcat(cmdstr, args[i]);
        }
        add_command_to_history(cmdstr);
        free(cmdstr);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child
        execvp(args[0], args);
        /* If execvp returns, it's an error */
        if (errno == ENOENT) {
            fprintf(stderr, "Command not found: %s\n", args[0]);
            _exit(127);
        } else {
            perror("Execution failed");
            _exit(EXIT_FAILURE);
        }
    } else {
        // Parent
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    }
}