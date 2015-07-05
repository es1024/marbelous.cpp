#ifndef CELL_H
#define CELL_H

#include <cstdint>

#include "devices.h"

struct BoardCall;
struct Cell{
	Cell() = default;
	Cell(Device device, uint8_t value);
	Cell(BoardCall *board_call);

	Device device;
	union{
		uint8_t value;
		BoardCall *board_call;
	};
};

#endif // CELL_H
