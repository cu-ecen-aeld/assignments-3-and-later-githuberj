# Variables
#CC = aarch64-none-linux-gnu-gcc
CROSS_COMPILE = 
CC = gcc
CFLAGS = -Wall -Wextra -I.
#DEPS = foo.h
OBJ = writer.o 
TARGET = writer

# Rule to create the final executable
$(TARGET): $(OBJ)
	$(CROSS_COMPILE)$(CC) -o $@ $^

# Rule to compile .c files to .o files
%.o: %.c $(DEPS)
	$(CROSS_COMPILE)$(CC) -c -o $@ $< $(CFLAGS)

# Clean up
.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)
