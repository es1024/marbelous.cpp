#ifndef LOAD_H
#define LOAD_H

#include "board.h"

#include <map>
#include <string>
#include <vector>

bool load_mbl_file(std::string file,
				   std::vector<Board> &boards,
				   std::map<std::string, unsigned> &lookup
				  );

#endif
