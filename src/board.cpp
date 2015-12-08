#include "board.h"
#include "cell.h"
#include "devices.h"
#include "emit.h"
#include "io_functions.h"
#include "options.h"

#include <algorithm>
#include <cstdio>
#include <utility>

BoardCall::BoardCall(Board *board, uint16_t x, uint16_t y): board(board), x(x), y(y){}

// check if a uint16_t represents a marble or an empty cell
static inline bool is_empty_cell(uint16_t value){
	return !(value & 0xFF00);
}

BoardCall::RunState *BoardCall::call(const BoardCall *bc, uint8_t inputs[], int indents){
	return bc->call(inputs, indents);
}

BoardCall::RunState *BoardCall::call(uint8_t inputs[], int indents) const {
	// prepare runstate
	RunState *rs = new_run_state(inputs, indents);

	if(verbosity > 2)
		rs->output_board();

	// run to completion
	while(rs->tick(false));

	rs->finalize();

	return rs;
}

BoardCall::RunState *BoardCall::new_run_state(uint8_t inputs[], int indents) const {
	// prepare runstate
	RunState *rs = new RunState;
	rs->bc = this;
	rs->indents = indents;
	// fill with empty cell placeholders
	rs->cur_marbles.resize(board->width * board->height, 0);
	rs->next_marbles.resize(board->width * board->height, 0);
	// initialize board values
	for(const std::pair<uint32_t, uint8_t> &marble : board->initial_marbles)
		rs->cur_marbles[marble.first] = marble.second | 0xFF00;
	for(int i = 0; i < 36; ++i)
		for(uint32_t loc : board->inputs[i])
			rs->cur_marbles[loc] = inputs[i] | 0xFF00;
	// set unused outputs to prefill
	for(int i = 0; i < 36; ++i)
		rs->outputs_filled[i] = board->outputs[i].empty();
	rs->left_filled = board->output_left.empty();
	rs->right_filled = board->output_right.empty();
	// if there are no outputs, then don't exit based on all outputs filled
	rs->no_output = std::find(rs->outputs_filled.begin(), rs->outputs_filled.end(), false) == rs->outputs_filled.end()
	                 && rs->left_filled && rs->right_filled;
	// reserve space for stdout
	rs->stdout_values.resize(board->width);

	return rs;
}

BoardCall::RunState::~RunState(){
	for(const auto rs : prepared_board_calls)
		delete rs;
	for(const auto rs : processed_board_calls)
		delete rs;
}

void BoardCall::RunState::prepare_board_calls(){
	for(const auto &board_call : bc->board->board_calls){
		uint32_t loc = bc->board->index(board_call.x, board_call.y);
		bool canCall = true;
		for(int i = 0; i < board_call.board->length; ++i)
			if(!board_call.board->inputs[i].empty() && is_empty_cell(cur_marbles[loc + i])){
				canCall = false;
				break;
			}
		if(!canCall){
			for(uint32_t i = loc, end = loc + board_call.board->length; i < end; ++i){
				if(!is_empty_cell(cur_marbles[i]))
					set_marble(i, 0, 0, cur_marbles[i]);
			}
		}else{
			uint8_t inputs[36] = { };
			for(int i = 0; i < board_call.board->length; ++i)
				inputs[i] = cur_marbles[loc + i] & 0xFF;
			RunState *rs = board_call.new_run_state(inputs, indents + 1);
			prepared_board_calls.push_back(rs);
		}
	}
}

bool BoardCall::RunState::tick(bool use_prepared){
	marbles_moved = false;
	if(use_prepared){
		for(const auto rs : processed_board_calls){
			uint32_t loc = bc->board->index(rs->bc->x, rs->bc->y);
			for(int i = 0; i < rs->bc->board->length; ++i)
				if(!is_empty_cell(rs->outputs[i]))
					set_marble(loc + i, 0, 1, rs->outputs[i]);
			if(!is_empty_cell(rs->output_left))
				set_marble(loc, -1, 0, rs->output_left);
			if(!is_empty_cell(rs->output_right))
				set_marble(loc + (rs->bc->board->length - 1), 1, 0, rs->output_right);
			marbles_moved = true;
		}
	}else{
		process_boardcalls();
	}
	// clear prepared/processed board calls
	for(const auto rs : prepared_board_calls)
		delete rs;
	for(const auto rs : processed_board_calls)
		delete rs;
	prepared_board_calls.clear();
	processed_board_calls.clear();

   	// movement through synchronisers and board calls cannot be 
   	// processed with only information about one marble
	process_synchronisers();
   	// deal with all other marbles
   	for(uint16_t y = 0; y < bc->board->height; ++y){
   		for(uint16_t x = 0; x < bc->board->width; ++x){
   			uint32_t index = bc->board->index(x,y);
   			const Cell &cell = bc->board->cells[index];
   			if(!is_empty_cell(cur_marbles[index])){
   				process_cell(x, y, cell);
   			}
   		}
   	}
	// next -> cur
	std::swap(cur_marbles, next_marbles);
	std::fill(next_marbles.begin(), next_marbles.end(), 0);
	// output stdout
	for(int i = 0; i < bc->board->width; ++i){
		if(!is_empty_cell(stdout_values[i])){
			stdout_write(stdout_values[i]);
			stdout_text.push_back(stdout_values[i] & 255);
			stdout_values[i] = 0;
		}
	}
	++tick_number;
	if(verbosity > 2)
		output_board();
	
	return !is_finished();
}

void BoardCall::RunState::finalize(){
	for(int i = 0, len = bc->board->length; i < len; ++i)
		copy_output_helper(outputs[i], bc->board->outputs[i]);
	copy_output_helper(output_left, bc->board->output_left);
	copy_output_helper(output_right, bc->board->output_right);
	if(verbosity > 1){
		std::string indent = std::string(indents, ' ');
		if(stdout_text.size() > 0){
			std::printf("%sstdout_write STDOUT:", indent.c_str());
			for(uint8_t c : stdout_text){
				_stdout_writehex(c);
			}
			std::fputc('\n', stdout);
		}
		std::printf("%sExiting board %s on tick %u due to ", indent.c_str(), bc->board->short_name.c_str(), tick_number);
		if(terminator_reached)
			std::puts("a filled terminator (!!) device");
		else if(!marbles_moved)
			std::puts("lack of activity");
		else
			std::puts("filled output devices");		
	}
}

bool BoardCall::RunState::is_finished(){
	return !((!terminator_reached) &&
	       (marbles_moved) &&
	       (no_output || (
	           std::find(outputs_filled.begin(), outputs_filled.end(), false) != outputs_filled.end() ||
	           (!left_filled) ||
	           (!right_filled)
	       )));
}

void BoardCall::RunState::output_board(){
	std::string indent = std::string(indents, ' ');
	std::printf("%s:%s tick %u\n", indent.c_str(), bc->board->short_name.c_str(), tick_number);
	for(int y = 0; y < bc->board->height; ++y){
		std::fputs(indent.c_str(), stdout);
		for(int x = 0; x < bc->board->width; ++x){
			uint32_t loc = bc->board->index(x, y);
			if(!is_empty_cell(cur_marbles[loc])){
				std::printf("%02X ", cur_marbles[loc] & 255);
			}else{
				uint8_t raw_value = bc->board->cells[loc].value;
				char value = '#';
				if(bc->board->cells[loc].device != DV_BOARD){
					if(raw_value < 10)
						value = raw_value + '0';
					else if(raw_value < 36)
						value = (raw_value - 10) + 'A';
					else if(raw_value == 253)
						value = '?';
					else if(raw_value == 254)
						value = '>';
					else if(raw_value == 255)
						value = '<';
				}
				switch(bc->board->cells[loc].device){
					case DV_LEFT_DEFLECTOR: std::fputs("// ", stdout); break;
					case DV_RIGHT_DEFLECTOR: std::fputs("\\\\ ", stdout); break;
					case DV_PORTAL: std::printf("@%c ", value); break;
					case DV_SYNCHRONISER: std::printf("&%c ", value); break;
					case DV_EQUALS: std::printf("=%c ", value); break;
					case DV_GREATER_THAN: std::printf(">%c ", value); break;
					case DV_LESS_THAN: std::printf("<%c ", value); break;
					case DV_ADDER: std::printf("+%c ", value); break;
					case DV_SUBTRACTOR: std::printf("-%c ", value); break;
					case DV_INCREMENTOR: std::fputs("++ ", stdout); break;
					case DV_DECREMENTOR: std::fputs("-- ", stdout); break;
					case DV_BIT_CHECKER: std::printf("^%c ", value); break;
					case DV_LEFT_BIT_SHIFTER: std::fputs("<< ", stdout); break;
					case DV_RIGHT_BIT_SHIFTER: std::fputs(">> ", stdout); break;
					case DV_BINARY_NOT: std::fputs("~~ ", stdout); break;
					case DV_STDIN: std::fputs("]] ", stdout); break;
					case DV_INPUT: std::printf("}%c ", value); break;
					case DV_OUTPUT: std::printf("{%c ", value); break;
					case DV_TRASH_BIN: std::fputs("\\/ ", stdout); break;
					case DV_CLONER: std::fputs("/\\ ", stdout); break;
					case DV_TERMINATOR: std::fputs("!! ", stdout); break;
					case DV_RANDOM: std::printf("?%c ", value); break;
					case DV_BOARD:{
						BoardCall *b = bc->board->cells[loc].board_call;
						int offset = x - b->x;
						std::string fragment = b->board->actual_name.substr(2 * offset, 2);
						std::printf("%s ", fragment.c_str());
						break;
					}
					default: std::fputs(".. ", stdout); break;
				}
			}
		}
		std::fputc('\n', stdout);
	}
	std::fputc('\n', stdout);
}

void BoardCall::RunState::copy_output_helper(uint16_t &output,
                                             const std::forward_list<uint32_t> &output_locs){
	output = 0;
	if(!output_locs.empty()){
		output = 0;
		bool filled = false;
		for(uint32_t loc : output_locs)
			if(!is_empty_cell(cur_marbles[loc]))
				output = (output + cur_marbles[loc]) & 0xFF, filled = true;
		output |= 0xFF00;
		if(!filled)
			output = 0;
	}
}
void BoardCall::RunState::set_marble(uint32_t loc,
                                     int32_t x_disp, 
                                     int32_t y_disp,
                                     uint16_t value){
	uint16_t x, y;
	y = loc / bc->board->width;
	x = loc % bc->board->width;
	if(x + x_disp >= bc->board->width || x + x_disp < 0){
		if(cylindrical){
			if(x + x_disp >= bc->board->width){
				x = 0;
			}else{
				x = bc->board->width - 1;
			}
			x_disp = 0;
		}else{
			return;
		}
	}
	if(y + y_disp < 0){
		emit_warning("Displacement of marble over top of board; this should never happen");
		return;
	}
	if(y + y_disp >= bc->board->height){
		stdout_values[x] = value | 0xFF00;
		return;
	}
	loc = bc->board->index(x + x_disp, y + y_disp);
	next_marbles[loc] = ((next_marbles[loc] + value) & 255) | 0xFF00;

	if(bc->board->cells[loc].device == DV_TERMINATOR){
		terminator_reached = true;
	}else if(bc->board->cells[loc].device == DV_OUTPUT){
		switch(bc->board->cells[loc].value){
			case 255: // left output
				left_filled = true;
			break;
			case 254: // right output
				right_filled = true;
			break;
			default: // value = n in {n	
				outputs_filled[bc->board->cells[loc].value] = true;
		}
	}
}
void BoardCall::RunState::process_synchronisers(){
	for(int i = 0; i < 36; ++i){
		bool allSet = true;
		for(uint32_t loc : bc->board->synchronisers[i])
			allSet &= !is_empty_cell(cur_marbles[loc]);
		if(allSet){
			for(uint32_t loc : bc->board->synchronisers[i]){
				// move down a row
				set_marble(loc, 0, 1, cur_marbles[loc]);
				marbles_moved = true;
			}
		}else{
			for(uint32_t loc : bc->board->synchronisers[i]){
				if(!is_empty_cell(cur_marbles[loc]))
					set_marble(loc, 0, 0, cur_marbles[loc]);
			}
		}
	}
}
void BoardCall::RunState::process_boardcalls(){
	for(const auto &board_call : bc->board->board_calls){
		uint32_t loc = bc->board->index(board_call.x, board_call.y);
		bool canCall = true;
		for(int i = 0; i < board_call.board->length; ++i)
			if(!board_call.board->inputs[i].empty() && is_empty_cell(cur_marbles[loc + i])){
				canCall = false;
				break;
			}
		if(!canCall){
			for(uint32_t i = loc, end = loc + board_call.board->length; i < end; ++i){
				if(!is_empty_cell(cur_marbles[i]))
					set_marble(i, 0, 0, cur_marbles[i]);
			}
		}else{
			uint8_t inputs[36] = { };
			for(int i = 0; i < board_call.board->length; ++i)
				inputs[i] = cur_marbles[loc + i] & 0xFF;
			RunState *rs = board_call.call(inputs, indents + 1);
			for(int i = 0; i < board_call.board->length; ++i)
				if(!is_empty_cell(rs->outputs[i]))
					set_marble(loc + i, 0, 1, rs->outputs[i]);
			if(!is_empty_cell(rs->output_left))
				set_marble(loc, -1, 0, rs->output_left);
			if(!is_empty_cell(rs->output_right))
				set_marble(loc + (board_call.board->length - 1), 1, 0, rs->output_right);
			marbles_moved = true;
			delete rs;
		}
	}
}
void BoardCall::RunState::process_cell(uint16_t x,
                                       uint16_t y,
                                       const Cell &cell){
	uint32_t loc = bc->board->index(x, y);
	uint16_t value = cur_marbles[loc] & 255;
	switch(cell.device){
		case DV_LEFT_DEFLECTOR: 
			set_marble(loc, -1, 0, value);
			marbles_moved = true;
		break;
		case DV_RIGHT_DEFLECTOR: 
			set_marble(loc, +1, 0, value);
			marbles_moved = true;
		break;
		case DV_PORTAL:
		{
			const auto &portals = bc->board->portals[cell.value];
			uint32_t out_loc;
			if(portals.size() == 1){
				// unpaired portal
				out_loc = loc;
			}else{
				// cannot exit out of entrance portal unless only 1 portal
				// if out_loc >= current index, add 1
				int out_portal = std::rand() % (portals.size() - 1); 
				if(out_portal >= std::distance(portals.begin(), std::find(portals.begin(), portals.end(), loc))){
					++out_portal;
				}
				out_loc = portals[out_portal];
			}
			set_marble(out_loc, 0, +1, value);
			marbles_moved = true;
		}
		break;
		case DV_EQUALS: 
			if(value == cell.value)
				set_marble(loc, 0, +1, value);
			else
				set_marble(loc, +1, 0, value);
			marbles_moved = true;
		break;
		case DV_GREATER_THAN:
			if(value > cell.value)
				set_marble(loc, 0, +1, value);
			else
				set_marble(loc, +1, 0, value);
			marbles_moved = true;
		break;
		case DV_LESS_THAN:
			if(value < cell.value)
				set_marble(loc, 0, +1, value);
			else
				set_marble(loc, +1, 0, value);
			marbles_moved = true;
		break;
		case DV_ADDER:
		case DV_INCREMENTOR:
			set_marble(loc, 0, +1, value + cell.value);
			marbles_moved = true;
		break;
		case DV_SUBTRACTOR:
		case DV_DECREMENTOR:
			set_marble(loc, 0, 1, value - cell.value);
			marbles_moved = true;
		break;
		case DV_BIT_CHECKER:
			set_marble(loc, 0, +1, !!(value & (1 << cell.value)));
			marbles_moved = true;
		break;
		case DV_LEFT_BIT_SHIFTER:
			set_marble(loc, 0, +1, value << 1);
			marbles_moved = true;
		break;
		case DV_RIGHT_BIT_SHIFTER:
			set_marble(loc, 0, +1, value >> 1);
			marbles_moved = true;
		break;
		case DV_BINARY_NOT:
			set_marble(loc, 0, +1, ~value);
			marbles_moved = true;
		break;
		case DV_STDIN:
			if(stdin_available())
				set_marble(loc, 0, +1, stdin_get());
			else
				set_marble(loc, +1, 0, value);
			marbles_moved = true;
		break;
		case DV_OUTPUT:
			set_marble(loc, 0, 0, value);
		break;
		case DV_TRASH_BIN:
			// marbles are to be removed, do nothing
			marbles_moved = true;
		break;
		case DV_CLONER:
			set_marble(loc, -1, 0, value);
			set_marble(loc, +1, 0, value);
			marbles_moved = true;
		break;
		case DV_TERMINATOR:
			terminator_reached |= true;
		break;
		case DV_RANDOM:
			if(cell.value == 253) // ?? device
				set_marble(loc, 0, +1, std::rand() % (value + 1u));
			else // ?n device
				set_marble(loc, 0, +1, std::rand() % (cell.value + 1));
			marbles_moved = true;
		break;
		case DV_BLANK:
		case DV_INPUT:
			set_marble(loc, 0, +1, value);
			marbles_moved = true;
		break;
		default: 
			// already processed, do nothing
		break;
	}
}

void Board::initialize(){
	// get highest number input used
	int highest_input = 0;
	for(int i = 36; i --> 0;){
		if(!inputs[i].empty()){
			highest_input = i;
			break;
		}
	}
	// get highest number output used
	int highest_output = 0;
	for(int i = 36; i --> 0;){
		if(!outputs[i].empty()){
			highest_output = i;
			break;
		}
	}
	length = std::max(1, std::max(highest_input, highest_output) + 1);
	// set actual_name
	actual_name = "";
	do actual_name += short_name; while(actual_name.length() < 2 * length);
	actual_name = actual_name.substr(0, 2 * length);
}

