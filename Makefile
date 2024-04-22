CC = gcc
CXX = g++
CFLAGS = -g -Wall
TARGET = fooddelivery

all: $(TARGET)

$(TARGET): main.cpp log.c
	$(CXX) $(CFLAGS) -o $(TARGET) main.cpp log.c -lpthread

clean:
	rm $(TARGET)
