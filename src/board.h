#ifndef BOARD_H
#define BOARD_H

#include "cell.h"

#include <cstdint>
#include <forward_list>
#include <string>
#include <vector>

class Board;

// list of locations on board calling
struct BoardCall{
	struct RunState;

	BoardCall() = default;
	BoardCall(Board *board, uint16_t x, uint16_t y);

	// inputs: must be at least the length of the board; fill with anything if unused
	// outputs, left_output, right_output: will be filled with 0x**XX if used (** nonzero)
	static RunState *call(const BoardCall *bc, uint8_t inputs[], int indents = 0);

	RunState *call(uint8_t inputs[], int indents = 0) const;
		
	RunState *new_run_state(uint8_t inputs[], int indents = 0) const;

	Board *board;
	uint16_t x, y; // location of first cell

	struct RunState{
		friend class BoardCall;

		~RunState();

		// sets prepared_board_calls
		// use process_boardcalls() instead if not concerned about debugging
		void prepare_board_calls();

		// copy over result of board calls
		// does a tick. note: does not process board calls.
		// returns true if board should not terminate after tick
		// use_prepared: false if not using prepare_board_calls
		bool tick(bool use_prepared = true);

		// aggregates outputs to output fields
		// call after board finishes.
		// do not call if not using prepare_board_calls
		void finalize();

		// check if board has terminated
		bool is_finished();

		std::vector<uint16_t> cur_marbles;
		std::vector<uint16_t> next_marbles;
		std::vector<uint8_t> stdout_text; // only used for verbose modes
		const BoardCall *bc;
		unsigned tick_number = 0;

		uint16_t outputs[36] = { };
		uint16_t output_left = 0, output_right = 0;

		std::vector<RunState *> prepared_board_calls;
		std::vector<RunState *> processed_board_calls;

		private:
			// internal states for when the board is running + not compiled
			// _outputs_filled and _*_output are true when output is filled or doesn't exist
			// outputs_filled and *_output are not true when the output does not exist
			bool marbles_moved = true, terminator_reached = false;
			std::vector<bool> outputs_filled = std::vector<bool>(36);
			bool left_filled = false, right_filled = false;
			std::vector<uint16_t> stdout_values;
			bool no_output = false; // exit if no outputs?
			int indents = 0;

			void output_board();
			void set_marble(uint32_t loc,
			                int32_t x_disp,
			                int32_t y_disp, 
			                uint16_t value);
			void process_synchronisers();
			void process_boardcalls();
			void process_cell(uint16_t x,
			                  uint16_t y,
			                  const Cell &cell);
			void copy_output_helper(uint16_t &output,
			                        const std::forward_list<uint32_t> &output_locs);
	};
};

struct Board{
	std::vector<Cell> cells; // 2d array, use index
	std::forward_list<std::pair<uint32_t, uint8_t>> initial_marbles;
	// these are labeled 0..Z (base 36 digits)
	std::forward_list<uint32_t> inputs[36], outputs[36], synchronisers[36];
	std::vector<uint32_t> portals[36];

	std::forward_list<uint32_t> output_left, output_right;
	std::forward_list<BoardCall> board_calls;

	uint16_t width, height;
	uint8_t length;

	std::string full_name; // file:line#boardName
	std::string actual_name; // boardNameboardNameboardNa..
	std::string short_name; // boardName

	bool initialized;

	void initialize();
	inline uint32_t index(uint16_t x, uint16_t y) const {
		return static_cast<uint32_t>(width) * y + x;
	}
};

#endif // BOARD_H
