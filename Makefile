# Simple Makefile to build the CRT overlay without needing CMake

CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Werror -pedantic
PKG_CONFIG ?= pkg-config

SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
SDL2_LIBS := $(shell $(PKG_CONFIG) --libs sdl2)

ifeq ($(strip $(SDL2_LIBS)),)
$(error SDL2 development files not found; install SDL2 and ensure pkg-config can find `sdl2`)
endif

GL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gl)
GL_LIBS := $(shell $(PKG_CONFIG) --libs gl)
ifeq ($(strip $(GL_LIBS)),)
GL_LIBS := -lGL
endif

X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags x11 xext 2>/dev/null)
X11_LIBS := $(shell $(PKG_CONFIG) --libs x11 xext 2>/dev/null)
X11_FLAGS :=
X11_LIBFLAGS :=
ifneq ($(strip $(X11_LIBS)),)
CXXFLAGS += -DCRT_ENABLE_X11
X11_FLAGS := $(X11_CFLAGS)
X11_LIBFLAGS := $(X11_LIBS)
endif

SOURCES := src/main.cpp
OBJECTS := $(SOURCES:.cpp=.o)
TARGET := crt

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(SDL2_LIBS) $(GL_LIBS) $(X11_LIBFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(SDL2_CFLAGS) $(GL_CFLAGS) $(X11_FLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
