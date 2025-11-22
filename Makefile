CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic $(shell pkg-config --cflags sdl2 gl x11 2>/dev/null)
LDFLAGS ?= $(shell pkg-config --libs sdl2 gl x11 2>/dev/null)
CXXFLAGS += -I/usr/include/X11
LDFLAGS += -lX11

TARGET := crt
SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
