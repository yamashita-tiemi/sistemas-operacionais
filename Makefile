CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_GNU_SOURCE -pthread
TARGET = process_manager
SOURCE = process_manager.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

debug: $(SOURCE)
	$(CC) $(CFLAGS) -g -DDEBUG -o $(TARGET) $(SOURCE)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

.PHONY: all clean run debug install