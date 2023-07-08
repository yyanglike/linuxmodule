CC = gcc
CFLAGS = -Wall -Wextra

TARGET = myprogram

all: $(TARGET)

$(TARGET): server.c
	$(CC) $(CFLAGS) -o $(TARGET) server.c

clean:
	rm -f $(TARGET)
