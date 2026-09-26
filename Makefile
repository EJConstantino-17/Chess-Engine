CC = gcc
CFLAGS = -Wall -Wextra -Iinc

SRCS = src/main.c src/bitboard.c src/movegen.c
OBJS = $(SRCS:.c=.o)
TARGET = main.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	cmd /c "if exist src\*.o del /q src\*.o"
	cmd /c "if exist $(TARGET) del /q $(TARGET)"
