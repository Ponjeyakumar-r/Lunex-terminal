CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LDFLAGS = -lpthread

TARGET = shell
SRCS = shell.c shell_web.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c shell.h shell_web.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	@echo "Starting custom shell... Type 'quit' to exit"
	@script -qc "./$(TARGET)" /dev/null || ./$(TARGET)

web: $(TARGET)
	@echo "Starting LUNEX Web Terminal..."
	@echo "Open http://localhost:8080 in your browser"
	@./$(TARGET) --web

test: $(TARGET)
	@echo "Running basic tests..."
	@echo "Test 1: pwd"
	@echo "pwd" | ./$(TARGET)
	@echo ""
	@echo "Test 2: echo"
	@echo "echo Hello World" | ./$(TARGET)
	@echo ""
	@echo "Test 3: ls"
	@echo "ls" | ./$(TARGET)

.PHONY: all clean run test web
