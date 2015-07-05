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
	BoardCall() = default;
	BoardCall(Board *board, uint16_t x, uint16_t y);

	void call();

	Board *board;
	uint16_t x, y; // location of first cell
	std::vector<uint8_t> inputs = std::vector<uint8_t>(36);
	std::vector<uint8_t> outputs = std::vector<uint8_t>(36);
	std::vector<bool> inputs_filled = std::vector<bool>(36);
	std::vector<bool> outputs_filled = std::vector<bool>(36);
	uint8_t left{0}, right{0};
	bool left_output{false}, right_output{false};
	private:
		// internal states for when the board is running
		// _outputs_filled and _*_output are true when output is filled or doesn't exist
		// outputs_filled and *_output are not true when the output does not exist
		bool _marbles_moved{false}, _terminator_reached{false};
		std::vector<bool> _outputs_filled = std::vector<bool>(36); // will be true if output doesn't exist
		bool _left_output{false}, _right_output{false};
		std::vector<uint8_t> _stdout;
		std::vector<bool> _stdout_filled;

		void set_marble(std::vector<uint8_t> &next_marbles,
						std::vector<bool> &next_filled,
						const Board *board,
						uint32_t loc,
						int32_t x_disp,
						int32_t y_disp, 
						uint8_t value);
		void process_synchronisers(const std::vector<uint8_t> &cur_marbles,
								   std::vector<uint8_t> &next_marbles,
								   const std::vector<bool> &cur_filled,
								   std::vector<bool> &next_filled);
		void process_boardcalls(const std::vector<uint8_t> &cur_marbles,
								std::vector<uint8_t> &next_marbles,
								const std::vector<bool> &cur_filled,
								std::vector<bool> &next_filled);
		void process_cell(uint16_t x,
						  uint16_t y,
						  uint8_t value,
						  const Cell &cell,
						  std::vector<uint8_t> &next_marbles,
						  std::vector<bool> &next_filled);
};

struct Board{
	std::vector<Cell> cells; // 2d array, use index
	std::forward_list<std::pair<uint32_t, uint8_t>> initial_marbles;
	// these are labeled 0..Z (base 36 digits)
	std::forward_list<uint32_t> inputs[36], outputs[36], synchronisers[36];
	std::vector<uint32_t> portals[36];

	std::forward_list<uint32_t> output_left, output_right;
	std::forward_list<BoardCall> boardCalls;

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
