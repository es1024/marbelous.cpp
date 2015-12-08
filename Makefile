CXX = g++
RM = rm -f

SRCS = src/board.cpp src/cell.cpp src/devices.cpp src/emit.cpp \
       src/io_functions.cpp src/load.cpp src/source_line.cpp
CSRCS = src/main.cpp
VSRCS = src/visual_main.cpp src/surfaces.cpp

OBJS = $(patsubst src/%.cpp, obj/%.o, $(SRCS))
COBJS = $(patsubst src/%.cpp, obj/%.o, $(CSRCS))
VOBJS = $(patsubst src/%.cpp, obj/%.o, $(VSRCS))

LIBS := $(shell pkg-config --cflags-only-other --libs gtk+-3.0 freetype2 pangoft2)
INCLUDES := $(shell pkg-config --cflags-only-I --libs gtk+-3.0 freetype2 pangoft2)
CXXFLAGS = -ggdb -Wall -std=c++11 -static-libstdc++

ifeq ($(OS), Windows_NT)
	BIN_SUFFIX = .exe
else
	BIN_SUFFIX =
endif

all: bin/marbelous$(BIN_SUFFIX) bin/vmarbelous$(BIN_SUFFIX)

bin/marbelous$(BIN_SUFFIX): $(OBJS) $(COBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

bin/vmarbelous$(BIN_SUFFIX): $(OBJS) $(VOBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES)

clean:
	$(RM) obj/*.o
	$(RM) bin/*$(BIN_SUFFIX)

