# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS = -g -Wall
LDFLAGS = -lSDL2main -lSDL2

# the build target executable:
TARGET = chip8

all: $(TARGET)

$(TARGET): main.cpp
	$(CC) $(CFLAGS) -o $(TARGET) main.cpp $(LDFLAGS)

clean:
	$(RM) $(TARGET)
