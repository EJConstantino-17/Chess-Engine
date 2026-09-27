CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -flto -Iinc -D_GNU_SOURCE

SRCS = src/main.c src/bitboard.c src/magic.c src/movegen.c src/perft.c src/eval.c src/search.c
OBJS = $(SRCS:.c=.o)
TARGET = main

ifeq ($(OS),Windows_NT)
    TARGET_BIN = $(TARGET).exe
else
    TARGET_BIN = $(TARGET)
endif

.PHONY: all clean

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-del /Q /S src\*.o $(TARGET_BIN) 2>NUL
else
	rm -f src/*.o $(TARGET_BIN)
endif
