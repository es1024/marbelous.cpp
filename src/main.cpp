#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include "board.h"
#include "emit.h"
#include "io_functions.h"
#include "load.h"
#include "options.h"

option::Option *options;
int verbosity;
bool cylindrical;

int main(int argc, char *argv[]){
	// process arguments
	option::Stats stats(usage, argc - 1, argv + 1);
	option::Option _options[stats.options_max], buffer[stats.buffer_max];
	option::Parser parse(usage, argc - 1, argv + 1, _options, buffer);

	options = _options;

	if(parse.error())
		return -1;
	
	if(options[OPT_HELP] || argc == 1){
		option::printUsage(std::cout, usage);
		return 0;
	}

	for(option::Option *opt = options[OPT_UNKNOWN]; opt; opt = opt->next())
		emit_warning(std::string("Unknown option: ") + opt->name);

	if(parse.nonOptionsCount() == 0){
		emit_error("No input file");
		return -2;
	}

	std::string filename = parse.nonOption(0);
	// load
	std::srand(std::time(nullptr));
	prepare_io(true);
	std::vector<Board> boards;
	std::map<std::string, unsigned> lookup;
	if(!load_mbl_file(filename, boards, lookup)){
		emit_error("Could not load file " + filename);
		return -3;
	}

	// get highest input
	int highest_input = -1;
	for(int i = 36; i --> 0;){
		if(!boards[0].inputs[i].empty()){
			highest_input = i;
			break;
		}
	}

	// check arguments
	if(parse.nonOptionsCount() != 2 + highest_input){ // filename + (highest_input + 1)
		emit_error("Expected " + std::to_string(highest_input + 1) + " inputs, got " + std::to_string(parse.nonOptionsCount() - 1));
		return -4;	
	}

	// misc options
	verbosity = options[OPT_VERBOSE].count();
	cylindrical = (options[OPT_CYLINDRICAL].last()->type() == OPT_TYPE_ENABLE);

	BoardCall bc{&boards[0], 0, 0};
	uint8_t inputs[36] = { 0 };

	for(int i = 0; i <= highest_input; ++i){
		if(!boards[0].inputs[i].empty()){
			std::string opt = parse.nonOption(i + 1);
			unsigned value = 0;
			bool too_large = false;
			for(auto c : opt){
				if(std::isdigit(c)){
					value = 10 * value + (c - '0');
					if(value > 255)
						too_large = true;
				}else{
					emit_error("Argument value " + opt + " is not a nonnegative integer");
				}
			}
			if(too_large){
				emit_warning("Argument value " + opt + " is larger than 255; using value mod 256");
			}
			inputs[i] = value & 255;
		}
	}

	// if verbose, stall printing to end..
	if(options[OPT_VERBOSE].count() > 0){
		stdout_write = _stdout_save;
	}

	BoardCall::RunState *rs = bc.call(inputs);

	if(options[OPT_VERBOSE].count() > 0){
		std::fputs("Combined STDOUT: ", stdout);
		for(uint8_t c : stdout_get_saved()){
			_stdout_writehex(c);
		}
		std::fputc('\n', stdout);
	}

	prepare_io(false);

	int res = (rs->outputs[0] >> 8) ? rs->outputs[0] & 0xFF : 0;

	delete rs;

	return res;
}
