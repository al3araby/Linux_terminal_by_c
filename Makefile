CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TARGET = $(BIN_DIR)/terminal_app

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/executor.c \
       $(SRC_DIR)/commands/exec_builtin.c \
       $(SRC_DIR)/commands/exec_external.c \
       $(SRC_DIR)/utils/logger.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@

# Separate rule for each object file to handle directories
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/executor.o: $(SRC_DIR)/executor.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/commands/exec_builtin.o: $(SRC_DIR)/commands/exec_builtin.c
	@mkdir -p $(OBJ_DIR)/commands
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/commands/exec_external.o: $(SRC_DIR)/commands/exec_external.c
	@mkdir -p $(OBJ_DIR)/commands
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/logger.o: $(SRC_DIR)/utils/logger.c
	@mkdir -p $(OBJ_DIR)/utils
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean