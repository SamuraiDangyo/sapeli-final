# Definitions

CC=clang
CFLAGS=-march=native -O3 -Wall -Wshadow -Wextra -pedantic -DNDEBUG
EXE=sapeli

# Targets

all:
	$(CC) $(CFLAGS) Sapeli.c -o $(EXE)

xboard:
	xboard -fUCI -fcp ./$(EXE)

clean:
	rm -f $(EXE)

.PHONY: all xboard clean
