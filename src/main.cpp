#include "board.h"
#include "io_functions.h"
#include "load.h"

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

int main(int argc, char *argv[]){
	std::srand(std::time(nullptr));
	prepare_io(true);
	// parse options..
	std::vector<Board> boards;
	std::map<std::string, unsigned> lookup;
	// todo: options...
	if(load_mbl_file(argv[1], boards, lookup)){
		// todo: boards with input
		BoardCall bc{&boards[lookup["MB"]], 0, 0};
		bc.call();
		std::puts("\nProgram finished.");
	}
	prepare_io(false);
}
