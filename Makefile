CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -g
SRC = ./src/
BIN = ./bin/

PROGS = $(BIN)myserver $(BIN)myclient

all:	$(PROGS)
$(BIN)%:	$(SRC)%.c
	$(CC) $< $(CFLAGS) -o $@ $(LIBS)

clean:
	rm $(BIN)*
