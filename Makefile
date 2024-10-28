CC = clang
CFLAGS = -Wall -g -I./include
LDFLAGS = -L./lib -lpq
TARGET = prog

all: $(TARGET)

$(TARGET): src/main.o
	$(CC) $(LDFLAGS) -o $@ $^

main.o: src/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET)
