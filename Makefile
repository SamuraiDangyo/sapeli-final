# Definitions

CC=clang
CFLAGS=-march=native -O2 -Wall -Wshadow -pedantic -Wextra -DNDEBUG
FILES=Sapeli.c
EXE=sapeli

# Targets

all:
	$(CC) $(CFLAGS) $(FILES) -o $(EXE)

clean:
	rm -f $(EXE)

.PHONY: all clean
