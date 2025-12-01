// exec_builtin.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "executor.h"

#define MAX_HISTORY 50

/* Global history buffer */
static char *command_history[MAX_HISTORY];
static int history_count = 0;
/* forward declaration for persistence helper */
static void persist_history_to_file(const char *command);
/* Add command to history - can be called from external functions */
void add_command_to_history(const char *command) {
    if (command == NULL || strlen(command) == 0) return;
    
    /* Skip history and cd commands in history display (meta commands) */
    if (strcmp(command, "history") == 0 || strcmp(command, "cd") == 0) {
        return;
    }
    
    if (history_count < MAX_HISTORY) {
        command_history[history_count] = malloc(strlen(command) + 1);
        if (command_history[history_count]) {
            strcpy(command_history[history_count], command);
            history_count++;
            /* Also persist to file */
            persist_history_to_file(command);
        }
    }
}

/* Persist history to a file in the user's home directory */
static void persist_history_to_file(const char *command) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.terminal_history", home);

    FILE *f = fopen(path, "a");
    if (!f) return; /* best-effort */
    fprintf(f, "%s\n", command);
    fclose(f);
}

/* Function to display about information */
int exec_about(char **args) {
    (void)args;
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║         C Linux Terminal Application v1.0                      ║\n");
    printf("║                                                                ║\n");
    printf("║  A simple terminal application written in C with built-in      ║\n");
    printf("║  command support and a graphical X11 interface.                ║\n");
    printf("║                                                                ║\n");
    printf("║  Features:                                                     ║\n");
    printf("║    - Execute Linux commands (external and built-in)            ║\n");
    printf("║    - Built-in commands for common tasks                        ║\n");
    printf("║    - Command history tracking                                  ║\n");
    printf("║    - File analysis tools                                       ║\n");
    printf("║    - GUI interface with X11                                    ║\n");
    printf("║                                                                ║\n");
    printf("║  Author: Mohamed EL3ARABY                                      ║\n");
    printf("║                                                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    fflush(stdout);
    return 0;  /* Return 0 on success for && operator compatibility */
}

/* Function to display help and available commands */
int exec_help(char **args) {
    (void)args;
    printf("\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("                    AVAILABLE COMMANDS\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("\nBUILT-IN COMMANDS:\n");
    printf("  about                - Display information about this application\n");
    printf("  help                 - Display this help message\n");
    printf("  clear                - Clear the terminal screen\n");
    printf("  cd <directory>       - Change the current directory\n");
    printf("  count <file>         - Count lines, words, and characters in a file\n");
    printf("  history              - Display command history\n");
    printf("  exit                 - Exit the terminal application\n");
    printf("\nEXTERNAL COMMANDS:\n");
    printf("  You can run any Linux command available on your system.\n");
    printf("  Examples: ls, echo, cat, grep, find, etc.\n");
    printf("\nEXAMPLES:\n");
    printf("  > echo Hello World\n");
    printf("  > ls -la\n");
    printf("  > count /path/to/file.txt\n");
    printf("  > history\n");
    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("\n");
    fflush(stdout);
    return 0;  /* Return 0 on success for && operator compatibility */
}

/* Function to clear the terminal screen */
int exec_clear(char **args) {
    (void)args;
    printf("\033[2J\033[H");  /* ANSI escape code to clear screen and move cursor to top */
    fflush(stdout);
    return 0;  /* Return 0 on success for && operator compatibility */
}

/* Function to count lines, words, and characters in a file */
int exec_count(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "count: missing argument - please provide a filename\n");
        fprintf(stderr, "Usage: count <filename>\n");
        return 1;
    }
    
    FILE *fp = fopen(args[1], "r");
    if (!fp) {
        fprintf(stderr, "count: cannot open file '%s'\n", args[1]);
        perror("fopen");
        return 1;
    }
    
    int lines = 0, words = 0, chars = 0;
    int in_word = 0;
    int c;
    
    while ((c = fgetc(fp)) != EOF) {
        chars++;
        
        if (c == '\n') {
            lines++;
            in_word = 0;
        } else if (c == ' ' || c == '\t' || c == '\n') {
            in_word = 0;
        } else if (!in_word) {
            words++;
            in_word = 1;
        }
    }
    
    /* Handle last line if file doesn't end with newline */
    if (in_word) {
        words++;
    }
    if (chars > 0 && c != '\n') {
        lines++;
    }
    
    fclose(fp);
    
    printf("\n");
    printf("File: %s\n", args[1]);
    printf("  Lines:      %d\n", lines);
    printf("  Words:      %d\n", words);
    printf("  Characters: %d\n", chars);
    printf("\n");
    fflush(stdout);
    
    return 0;  /* Return 0 on success for && operator compatibility */
}

/* Function to display command history */
int exec_history(char **args) {
    (void)args;
    if (history_count == 0) {
        printf("\nNo command history yet.\n\n");
        fflush(stdout);
        return 0;  /* Return 0 on success (even if empty) for && operator compatibility */
    }
    
    printf("\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("                    COMMAND HISTORY\n");
    printf("════════════════════════════════════════════════════════════════\n");
    
    for (int i = 0; i < history_count; i++) {
        printf("  %3d. %s\n", i + 1, command_history[i]);
    }
    
    printf("════════════════════════════════════════════════════════════════\n");
    printf("\n");
    fflush(stdout);
    
    return 0;  /* Return 0 on success for && operator compatibility */
}

/* Function to change the current directory */
int exec_cd(char **args) {
    const char *path;
    // Check if the user provided a directory
    if (args[1] == NULL) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        if(chdir(path) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }
    
    // Change the directory using chdir
    if (chdir(args[1]) != 0) {
        perror("cd"); // Print error if chdir fails
    }
    return 0;  /* Return 0 on success for && operator compatibility */
}

/* Function to exit the shell */
int exec_exit(char **args) {
    (void)args;
    return 0; // Return 0 to indicate exit
}

/* Function to execute built-in commands */
int exec_builtin(char **args) {
    if (args[0] == NULL) {
        return 1;
    }
    
    /* Add non-control commands to history */
    add_command_to_history(args[0]);
    
    // Check for built-in commands
    if (strcmp(args[0], "about") == 0) {
        return exec_about(args);
    } else if (strcmp(args[0], "help") == 0) {
        return exec_help(args);
    } else if (strcmp(args[0], "clear") == 0) {
        return exec_clear(args);
    } else if (strcmp(args[0], "count") == 0) {
        return exec_count(args);
    } else if (strcmp(args[0], "history") == 0) {
        return exec_history(args);
    } else if (strcmp(args[0], "cd") == 0) {
        return exec_cd(args);
    } else if (strcmp(args[0], "exit") == 0) {
        return exec_exit(args);
    }
    
    return 1; // Return 1 if no built-in command matched
}