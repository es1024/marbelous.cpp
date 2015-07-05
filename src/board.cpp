#include "board.h"
#include "cell.h"
#include "devices.h"
#include "emit.h"
#include "io_functions.h"

#include <algorithm>
#include <utility>

BoardCall::BoardCall(Board *board, uint16_t x, uint16_t y): board(board), x(x), y(y){}

// copy output helper for BoardCall::call
static inline bool copy_output_helper(const std::vector<uint8_t> &cur_marbles,
									const std::vector<bool> &cur_filled,
									uint8_t &value,
									const std::forward_list<uint32_t> &output);

void BoardCall::call(){
	// allocate space
	std::vector<uint8_t> cur_marbles(board->width * board->height),
						 next_marbles(board->width * board->height);
	std::vector<bool> cur_filled(board->width * board->height),
					  next_filled(board->width * board->height);
	// initialize board values
	for(const std::pair<uint32_t, uint8_t> &marble : board->initial_marbles)
		cur_marbles[marble.first] += marble.second, cur_filled[marble.first] = true;
	for(int i = 0; i < 36; ++i)
		if(inputs_filled[i])
			for(uint32_t loc : board->inputs[i])
				cur_marbles[loc] += inputs[i], cur_filled[loc] = true;
	// set unused outputs to prefill
	for(int i = 0; i < 36; ++i)
		_outputs_filled[i] = board->outputs[i].empty();
	_left_output = board->output_left.empty();
	_right_output = board->output_right.empty();
	// if there are no outputs, then don't exit based on all outputs filled
	bool no_output = std::find(_outputs_filled.begin(), _outputs_filled.end(), false) == _outputs_filled.end()
						&& _left_output && _right_output;
	_terminator_reached = false;
	// reserve space for stdout
	_stdout.resize(board->width);
	_stdout_filled.resize(board->width);
	// run to completion
	do{
		_marbles_moved = false;
	   	// movement through synchronisers and board calls cannot be 
	   	// processed with only information about one marble
		process_synchronisers(cur_marbles, next_marbles, cur_filled, next_filled);
		process_boardcalls(cur_marbles, next_marbles, cur_filled, next_filled);
	   	// deal with all other marbles
	   	for(uint16_t y = 0; y < board->height; ++y){
	   		for(uint16_t x = 0; x < board->width; ++x){
	   			uint32_t index = board->index(x,y);
	   			uint8_t value = cur_marbles[index];
	   			const Cell &cell = board->cells[index];
	   			if(cur_filled[index]){
	   				process_cell(x, y, value, cell, next_marbles, next_filled);
	   			}
	   		}
	   	}
		// next -> cur
		std::swap(cur_marbles, next_marbles);
		std::swap(cur_filled, next_filled);
		std::fill(next_marbles.begin(), next_marbles.end(), 0);
		std::fill(next_filled.begin(), next_filled.end(), false);
		// output stdout..
		for(int i = 0; i < board->width; ++i){
			if(_stdout_filled[i]){
				stdout_write(_stdout[i]);
				_stdout_filled[i] = false;
			}
		}
	}while(
		(!_terminator_reached) &&
		(_marbles_moved) &&
		(no_output || (
			std::find(_outputs_filled.begin(), _outputs_filled.end(), false) != _outputs_filled.end() ||
			!_left_output ||
			!_right_output
		))
	);

	for(int i = 0, len = board->length; i < len; ++i)
		outputs_filled[i] = copy_output_helper(cur_marbles, cur_filled, outputs[i], board->outputs[i]);
	left_output = copy_output_helper(cur_marbles, cur_filled, left, board->output_left);
	right_output = copy_output_helper(cur_marbles, cur_filled, right, board->output_right);
}

static inline bool copy_output_helper(const std::vector<uint8_t> &cur_marbles,
									const std::vector<bool> &cur_filled,
									uint8_t &value,
									const std::forward_list<uint32_t> &output){
	value = 0;
	for(uint32_t loc : output)
		if(cur_filled[loc])
			value += cur_marbles[loc];
	return !output.empty();
}
void BoardCall::set_marble(std::vector<uint8_t> &next_marbles,
						   std::vector<bool> &next_filled,
						   const Board *board,
						   uint32_t loc,
						   int32_t x_disp, 
						   int32_t y_disp,
						   uint8_t value){
	uint16_t x, y;
	y = loc / board->width;
	x = loc % board->width;
	if(x + x_disp >= board->width || x + x_disp < 0) return;
	if(y + y_disp < 0){
		emit_warning("Displacement of marble over top of board; this should never happen");
		return;
	}
	if(y + y_disp >= board->height){
		_stdout[x] = value;
		_stdout_filled[x] = true;
		return;
	}
	loc = board->index(x + x_disp, y + y_disp);
	next_marbles[loc] += value;
	next_filled[loc] = true;

	if(board->cells[loc].device == DV_TERMINATOR){
		_terminator_reached = true;
	}else if(board->cells[loc].device == DV_OUTPUT){
		switch(board->cells[loc].value){
			case 255: // left output
				_left_output = true;
			break;
			case 254: // right output
				_right_output = true;
			break;
			default: // value = n in {n	
				_outputs_filled[board->cells[loc].value] = true;
		}
	}
}
void BoardCall::process_synchronisers(const std::vector<uint8_t> &cur_marbles,
									  std::vector<uint8_t> &next_marbles,
									  const std::vector<bool> &cur_filled,
									  std::vector<bool> &next_filled){
	for(int i = 0; i < 36; ++i){
		bool allSet = true;
		for(uint32_t loc : board->synchronisers[i])
			allSet &= cur_filled[loc];
		if(allSet){
			for(uint32_t loc : board->synchronisers[i]){
				// move down a row
				set_marble(next_marbles, next_filled, board, loc, 0, 1, cur_marbles[loc]);
				_marbles_moved = true;
			}
		}else{
			for(uint32_t loc : board->synchronisers[i]){
				if(cur_filled[loc])
					set_marble(next_marbles, next_filled, board, loc, 0, 0, cur_marbles[loc]);
			}
		}
	}
}
void BoardCall::process_boardcalls(const std::vector<uint8_t> &cur_marbles,
								   std::vector<uint8_t> &next_marbles,
								   const std::vector<bool> &cur_filled,
								   std::vector<bool> &next_filled){
	for(const auto &boardCall : board->boardCalls){
		uint32_t loc = board->index(boardCall.x, boardCall.y);
		bool canCall = true;
		for(int i = 0; i < boardCall.board->length; ++i)
			if(!boardCall.board->inputs[i].empty() && !cur_filled[loc + i]){
				canCall = false;
				break;
			}
		if(!canCall){
			for(uint32_t i = loc, end = loc + boardCall.board->length; i < end; ++i){
				if(cur_filled[i])
					set_marble(next_marbles, next_filled, board, i, 0, 0, cur_marbles[i]);
			}
		}else{
			BoardCall bccopy = boardCall;
			for(int i = 0; i < 36; ++i)
				if(!bccopy.board->inputs[i].empty())
					bccopy.inputs[i] = cur_marbles[loc + i], bccopy.inputs_filled[i] = cur_filled[loc + i];
			bccopy.call();
			for(int i = 0; i < 36; ++i)
				if(bccopy.outputs_filled[i])
					set_marble(next_marbles, next_filled, board, loc + i, 0, 1, bccopy.outputs[i]);
			if(bccopy.left_output)
				set_marble(next_marbles, next_filled, board, loc, -1, 0, bccopy.left);
			if(bccopy.right_output)
				set_marble(next_marbles, next_filled, board, loc, bccopy.board->length, 0, bccopy.right);
			_marbles_moved = true;
		}
	}
}
void BoardCall::process_cell(uint16_t x,
							 uint16_t y,
							 uint8_t value,
							 const Cell &cell,
							 std::vector<uint8_t> &next_marbles,
							 std::vector<bool> &next_filled){
	uint32_t loc = board->index(x, y);
	switch(cell.device){
		case DV_LEFT_DEFLECTOR: 
			set_marble(next_marbles, next_filled, board, loc, -1, 0, value);
			_marbles_moved = true;
		break;
		case DV_RIGHT_DEFLECTOR: 
			set_marble(next_marbles, next_filled, board, loc, +1, 0, value);
			_marbles_moved = true;
		break;
		case DV_PORTAL:
		{
			const auto &portals = board->portals[cell.value];
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
			set_marble(next_marbles, next_filled, board, out_loc, 0, +1, value);
			_marbles_moved = true;
		}
		break;
		case DV_EQUALS: 
			if(value == cell.value)
				set_marble(next_marbles, next_filled, board, loc, 0, +1, value);
			else
				set_marble(next_marbles, next_filled, board, loc, +1, 0, value);
			_marbles_moved = true;
		break;
		case DV_GREATER_THAN:
			if(value > cell.value)
				set_marble(next_marbles, next_filled, board, loc, 0, +1, value);
			else
				set_marble(next_marbles, next_filled, board, loc, +1, 0, value);
			_marbles_moved = true;
		break;
		case DV_LESS_THAN:
			if(value < cell.value)
				set_marble(next_marbles, next_filled, board, loc, 0, +1, value);
			else
				set_marble(next_marbles, next_filled, board, loc, +1, 0, value);
			_marbles_moved = true;
		break;
		case DV_ADDER:
		case DV_INCREMENTOR:
			set_marble(next_marbles, next_filled, board, loc, 0, +1, value + cell.value);
			_marbles_moved = true;
		break;
		case DV_SUBTRACTOR:
		case DV_DECREMENTOR:
			set_marble(next_marbles, next_filled, board, loc, 0, 1, value - cell.value);
			_marbles_moved = true;
		break;
		case DV_BIT_CHECKER:
			set_marble(next_marbles, next_filled, board, loc, 0, +1, value & (1 << cell.value));
			_marbles_moved = true;
		break;
		case DV_LEFT_BIT_SHIFTER:
			set_marble(next_marbles, next_filled, board, loc, 0, +1, value << 1);
			_marbles_moved = true;
		break;
		case DV_RIGHT_BIT_SHIFTER:
			set_marble(next_marbles, next_filled, board, loc, 0, +1, value >> 1);
			_marbles_moved = true;
		break;
		case DV_BINARY_NOT:
			set_marble(next_marbles, next_filled, board, loc, 0, +1, ~value);
			_marbles_moved = true;
		break;
		case DV_STDIN:
			if(stdin_available())
				set_marble(next_marbles, next_filled, board, loc, 0, +1, stdin_get());
			else
				set_marble(next_marbles, next_filled, board, loc, +1, 0, value);
			_marbles_moved = true;
		break;
		case DV_OUTPUT:
			set_marble(next_marbles, next_filled, board, loc, 0, 0, value);
		break;
		case DV_TRASH_BIN:
			// marbles are to be removed, do nothing
			_terminator_reached |= false;
			_marbles_moved = true;
		break;
		case DV_CLONER:
			set_marble(next_marbles, next_filled, board, loc, -1, 0, value);
			set_marble(next_marbles, next_filled, board, loc, +1, 0, value);
			_marbles_moved = true;
		break;
		case DV_TERMINATOR:
			_terminator_reached |= true;
		break;
		case DV_RANDOM:
			if(cell.value == 255) // ?? device
				set_marble(next_marbles, next_filled, board, loc, 0, +1, std::rand() % value);
			else // ?n device
				set_marble(next_marbles, next_filled, board, loc, 0, +1, std::rand() % cell.value);
			_marbles_moved = true;
		break;
		case DV_BLANK:
		case DV_INPUT:
			set_marble(next_marbles, next_filled, board, loc, 0, +1, value);
			_marbles_moved = true;
		break;
		default: 
			// already processed, do nothing
		break;
	}
}

void Board::initialize(){
	// get highest number input used
	int highestInput = 0;
	for(int i = 36; i --> 0;){
		if(!inputs[i].empty()){
			highestInput = i;
			break;
		}
	}
	// get highest number output used
	int highestOutput = 0;
	for(int i = 36; i --> 0;){
		if(!outputs[i].empty()){
			highestOutput = i;
			break;
		}
	}
	length = std::max(1, std::max(highestInput, highestOutput));
	// set actual_name
	actual_name = "";
	do actual_name += short_name; while(actual_name.length() < 2 * length);
	actual_name = actual_name.substr(0, 2 * length);
}

