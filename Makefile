CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic $(shell pkg-config --cflags sdl2 gl 2>/dev/null)
LDFLAGS ?= $(shell pkg-config --libs sdl2 gl 2>/dev/null)

TARGET := crt
SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
