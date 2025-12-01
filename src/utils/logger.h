// logger.h
// This header file declares the logging functions for the terminal application.
// It allows other files to utilize the logging functionality for debugging and tracking execution flow.

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Logging helpers (varargs signatures match implementation in logger.c)
void log_message(const char *format, ...);
void log_error(const char *format, ...);

#endif // LOGGER_H