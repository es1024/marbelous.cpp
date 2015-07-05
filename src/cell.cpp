#include "cell.h"

Cell::Cell(Device device, uint8_t value){
	this->device = device;
	this->value = value;
}

Cell::Cell(BoardCall *board_call){
	this->device = DV_BOARD;
	this->board_call = board_call;
}
