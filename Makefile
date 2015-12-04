CXX = g++
RM = rm -f

SRCS = $(patsubst src/%main.cpp, , $(wildcard src/*.cpp))
OBJS = $(patsubst src/%.cpp, obj/%.o, $(SRCS))

LIBS := $(shell pkg-config --cflags --libs gtk+-3.0)
CXXFLAGS = -ggdb -Wall -std=c++11 -static-libstdc++

ifeq ($(OS), Windows_NT)
	BIN_SUFFIX = .exe
else
	BIN_SUFFIX =
endif

TARGET = bin/
ifeq ($(OS), Windows_NT)
	TARGET = bin/marbelous.exe bin
else
	TARGET = bin/marbelous
endif

all: bin/marbelous$(BIN_SUFFIX) bin/vmarbelous$(BIN_SUFFIX)

bin/marbelous$(BIN_SUFFIX): $(OBJS) obj/main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

bin/vmarbelous$(BIN_SUFFIX): $(OBJS) obj/visual_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(LIBS)

clean:
	$(RM) obj/*
	$(RM) bin/*

