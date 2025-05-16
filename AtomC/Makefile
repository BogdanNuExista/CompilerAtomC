# Compiler and flags
CC = gcc
CFLAGS = -Wall

# Output executable
OUTPUT = p

# Source files
SRC = main.c lexer.c utils.c parser.c ad.c vm.c at.c

# Default target
all: $(OUTPUT)
	$(OUTPUT).exe .\tests\testat.c

# Build the executable
$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC)

# Clean target to remove the executable and output file
clean:
	del $(OUTPUT).exe