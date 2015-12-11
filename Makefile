CXX = g++
RM = rm -f

SRCS = src/cell.cpp src/devices.cpp src/emit.cpp \
       src/io_functions.cpp src/load.cpp src/source_line.cpp
CSRCS = src/main.cpp src/board.cpp 
VSRCS = src/visual_main.cpp src/surfaces.cpp src/board.cpp 

OBJS = $(patsubst src/%.cpp, obj/%.o, $(SRCS))
COBJS = $(patsubst src/%.cpp, obj/%-c.o, $(CSRCS))
VOBJS = $(patsubst src/%.cpp, obj/%-v.o, $(VSRCS))

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
	$(CXX) $(CXXFLAGS) -DVMARBELOUS=1 -o $@ $^ $(LIBS)

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES)

obj/%-c.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES)

obj/%-v.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -DVMARBELOUS=1 -c -o $@ $^ $(INCLUDES)

clean:
	$(RM) obj/*.o
	$(RM) bin/*$(BIN_SUFFIX)

