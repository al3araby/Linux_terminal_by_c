// logger.c
#include <stdio.h>
#include <stdarg.h>
#include "logger.h"

// Function to log messages to the console
void log_message(const char *format, ...) {
    va_list args; // Initialize a variable argument list
    va_start(args, format); // Start processing the variable arguments

    // Print the formatted message to the console
    vprintf(format, args);
    printf("\n"); // Print a newline for better readability

    va_end(args); // Clean up the variable argument list
}

// Function to log error messages
void log_error(const char *format, ...) {
    va_list args; // Initialize a variable argument list
    va_start(args, format); // Start processing the variable arguments

    // Print the formatted error message to the console with "ERROR: " prefix
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n"); // Print a newline for better readability

    va_end(args); // Clean up the variable argument list
}