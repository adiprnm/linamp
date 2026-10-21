# Local development build (Arch/Fedora/Debian with SDL3 + FFmpeg dev packages).
#   make            -> build/linamp
#   make run        -> build + run
#   make clean
# CI / release builds use CMakeLists.txt instead.

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g0 -Wall -Wextra -Wno-unused-parameter
PKGS      = sdl3 libavformat libavcodec libavutil libswresample

PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,build/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: build/linamp

build/linamp: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(PKG_LIBS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -MMD -MP $(PKG_CFLAGS) -c $< -o $@

run: build/linamp
	./build/linamp

clean:
	rm -rf build

-include $(DEPS)

.PHONY: all run clean
