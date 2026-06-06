# Americano Makefile

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -O2
TARGET   = ../bin/americano

SRCS = main.c set.c cover.c pla_read.c pla_write.c espresso.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | ../bin
	$(CC) $(OBJS) -o $(TARGET)


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


../bin:
	mkdir -p ../bin


run: $(TARGET)
	$(TARGET)

# 清理
clean:
	rm -f $(OBJS) $(TARGET)
