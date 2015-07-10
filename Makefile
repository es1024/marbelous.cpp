CXX = g++
RM = rm -f
CXXFLAGS = -O3 -Wall -std=c++11 -static-libstdc++

SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp, obj/%.o, $(SRCS))

ifeq ($(OS), Windows_NT)
	TARGET = bin/marbelous.exe
else
	TARGET = bin/marbelous
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^

clean:
	$(RM) obj/*
	$(RM) bin/*

