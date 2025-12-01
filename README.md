# c-linux-terminal-app

This project is a simple Linux terminal application that allows users to execute any Linux command. It serves as an educational tool to understand how command execution works in a Unix-like environment.

## Project Structure

```
c-linux-terminal-app
├── src
│   ├── main.c               # Entry point of the application
│   ├── executor.c           # Command execution logic
│   ├── executor.h           # Header for executor functions
│   ├── commands
│   │   ├── exec_builtin.c   # Built-in command execution
│   │   └── exec_external.c   # External command execution
│   └── utils
│       ├── logger.c         # Logging utility functions
│       └── logger.h         # Header for logging functions
├── include
│   └── config.h             # Configuration constants and macros
├── tests
│   └── test_executor.c      # Unit tests for command execution
├── Makefile                 # Build instructions
├── .gitignore               # Files to ignore in version control
└── README.md                # Project documentation
```

## Features

- Execute built-in commands (e.g., `cd`, `exit`).
- Execute external commands using the `exec` family of functions.
- Logging functionality to track command execution and errors.
- Unit tests to ensure the correctness of command execution logic.

## Building the Project

To build the project, navigate to the project directory and run:

```
make
```

This will compile the source files and create the executable.

## Running the Application

After building, you can run the terminal application with:

```
./c-linux-terminal-app
```

## GUI TERMINAL

you have 2 gui 
pyhton GUI 
```
python3 gui_terminal.py 
```
C GUI
```
./gui_terminal.c
```
