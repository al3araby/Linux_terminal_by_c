#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <sys/types.h>
#include "executor.h"

#define BUFFER_SIZE 1024

// Function to initialize the terminal application
void initialize_terminal() {
    // Print a welcome message
    printf("Welcome to OUR C Linux Terminal App!\n");
    printf("Type 'exit' to quit the application.\n");
}

// Function to print a colored prompt with username@hostname:cwd$
static void print_prompt() {
    char host[256];
    char cwd[1024];
    const char *user = NULL;
    struct passwd *pw = getpwuid(getuid());
    if (pw) user = pw->pw_name; else user = "user";
    if (gethostname(host, sizeof(host)) != 0) strcpy(host, "host");
    if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, "~");

    /* colored: user@host in green, cwd in blue */
    printf("\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", user, host, cwd);
    fflush(stdout);
}

// Function to read user input from the terminal
void read_user_input(char *buffer) {
    print_prompt();
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
        /* EOF or error */
        buffer[0] = '\0';
        return;
    }
    buffer[strcspn(buffer, "\n")] = 0; // Remove the newline character
}

// Main function - entry point of the application
int main() {
    char input[BUFFER_SIZE]; // Buffer to hold user input

    initialize_terminal(); // Initialize the terminal

    while (1) {
        read_user_input(input); // Read user input

        if (strlen(input) == 0) {
            /* Empty input or EOF: continue or exit on EOF */
            if (feof(stdin)) break;
            continue;
        }

        // Check for exit command
        if (strcmp(input, "exit") == 0) {
            break; // Exit the loop if user types 'exit'
        }

        // Execute the command entered by the user
        execute_command(input); // Call the command executor
    }

    printf("Exiting the terminal application. Goodbye!\n");
    return 0; // Return success
}