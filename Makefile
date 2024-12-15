CC = gcc
CFLAGS = -Wall -Wextra -Werror -O3

SRC = src/
INCLUDE = include/
BIN = bin/

# Targets
.PHONY: all
all: $(BIN)/download

$(BIN)/download: main.c $(SRC)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE)

.PHONY: run
run: $(BIN)/download
	./$(BIN)/download $(ARG)

.PHONY: clean
clean:
	rm -f $(BIN)/download

.PHONY: debug
debug: CFLAGS += -g
debug: $(BIN)/main

.PHONY: valgrind
valgrind: $(BIN)/download
	valgrind --leak-check=full ./$(BIN)/download $(ARG)

.PHONY: run_rx_val
run_rx_val: $(BIN)/main
	valgrind --leak-check=full ./$(BIN)/download $(ARG)