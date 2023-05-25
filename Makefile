
SRC = $(wildcard src/*.c)
OBJ = $(subst src/,build/,$(SRC:.c=.o))

CC = gcc
LD = gcc
CFLAGS = -Wall -Wextra -O2
LD_FLAGS = -lcurses -lm
EXEC = build/pipeview-ncurses

all: $(EXEC)

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXEC): $(OBJ)
	@mkdir -p $(@D)
	$(LD) $(CFLAGS) $^ -o $@ $(LD_FLAGS)

.phony: clean
clean:
	rm -r build

