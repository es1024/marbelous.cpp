#include "board.h"
#include "cell.h"
#include "devices.h"
#include "emit.h"
#include "io_functions.h"
#include "load.h"
#include "source_line.h"
#include "trim.h"

#include <algorithm>
#include <cstring>
#include <forward_list>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

// helper functions..
static inline bool _load_file(std::string file, std::list<SourceLine> &lines);
static inline bool _names_equivalent(const std::string &name1, const std::string &name2);
static inline bool _strip_blank_lines(std::list<SourceLine> &lines);
static inline void _process_cell(const std::string &cell, unsigned pos, Board &board);
static inline bool _load_board(const std::list<SourceLine> &lines,
							   Board &board);
static inline bool _load_boards(std::list<SourceLine> &lines, 
								std::vector<Board> &boards, // global
								std::map<std::string, unsigned> &self_ids, // lookup for this file's boards
								std::map<std::string, unsigned> &include_ids, // lookup for included file's boards
								std::map<unsigned, std::list<SourceLine>> &sources, // sources to process
								std::string filename
								);
static inline bool _resolve_board_calls(std::vector<Board> &boards,
										const std::map<unsigned, std::list<SourceLine>> &board_sources,
										const std::map<std::string, unsigned> &lookup,
										const std::map<std::string, unsigned> &include_lookup
										);

const std::string include_prefix = "#include";

static inline bool _load_file(std::string file, std::list<SourceLine> &lines){
	unsigned line_number = 0;
	std::fstream fs;

	fs.open(file.c_str(), std::ios_base::in);
	if(!fs.fail()){
		for(std::string line; std::getline(fs, line);){
			lines.push_back(SourceLine(file, ++line_number, line));
		}
		fs.close();
		return true;
	}else{
		emit_error("Error reading from file `" + file + "`: `" + std::strerror(errno) + "`");
		return false;
	}
}

static inline bool _names_equivalent(const std::string &name1, const std::string &name2){
	if(name1.length() == 0 || name2.length() == 0) return false;

	std::string::const_iterator sbeg, lbeg, send, lend;
	if(name1.length() < name2.length()){
		sbeg = name1.begin();
		send = name1.end();
		lbeg = name2.begin();
		lend = name2.end();
	}else{
		sbeg = name2.begin();
		send = name2.end();
		lbeg = name1.begin();
		lend = name1.end();
	}

	std::string::const_iterator sitr = sbeg, litr = lbeg;
	while(litr != lend && *litr++ == *sitr++)
		if(sitr == send)
			sitr = sbeg;
	return litr == lend;
}
static inline bool _strip_blank_lines(std::list<SourceLine> &lines){
	auto itr = lines.begin();
	while(itr != lines.end()){
		std::string stripped = itr->get_stripped();
		if(!itr->is_include() && trim_left(stripped) == "")
			lines.erase(itr++);
		else 
			++itr;
	}
	return true;
}
static inline std::string _make_full_name(std::string file, unsigned line, std::string name){
	return file + ":" + std::to_string(line) + "#" + name;
}
static inline bool _is_base36(char x){
	return ('0' <= x && x <= '9') || ('A' <= x && x <= 'Z');
}
static inline void _process_cell(const std::string &cell, unsigned pos, Board &board){
	if(cell.find_first_not_of("0123456789ABCDEF") == std::string::npos){
		board.cells[pos] = Cell(DV_BLANK, 0);
		board.initial_marbles.push_front(std::pair<uint32_t, uint8_t>{
			pos,
			std::stoi(cell, nullptr, 16)
		});
	}else if(cell == ".." || cell == "  "){
		board.cells[pos] = Cell(DV_BLANK, 0);
	}else{
		// try to match device; fall back on board call if necessary
		Device device = DV_BOARD;
		uint8_t value = cell[1];
		value = ('0' <= value && value <= '9') ? (value - '0') : (value - 'A' + 10);
		switch(cell[0]){
			case '/': if(cell[1] == '/') device = DV_LEFT_DEFLECTOR;
					  else if(cell[1] == '\\') device = DV_CLONER; break;
			case '\\': if(cell[1] == '\\') device = DV_RIGHT_DEFLECTOR;
					   else if(cell[1] == '/') device = DV_TRASH_BIN; break;
			case '@': if(_is_base36(cell[1])) device = DV_PORTAL; break;
			case '&': if(_is_base36(cell[1])) device = DV_SYNCHRONISER; break;
			case '=': if(_is_base36(cell[1])) device = DV_EQUALS; break;
			case '>': if(_is_base36(cell[1])) device = DV_GREATER_THAN;
					  else if(cell[1] == '>') device = DV_RIGHT_BIT_SHIFTER; break;
			case '<': if(_is_base36(cell[1])) device = DV_LESS_THAN;
					  else if(cell[1] == '<') device = DV_LEFT_BIT_SHIFTER; break;
			case '+': if(_is_base36(cell[1])) device = DV_ADDER;
					  else if(cell[1] == '+') device = DV_INCREMENTOR, value = 1; break;
			case '-': if(_is_base36(cell[1])) device = DV_SUBTRACTOR;
			          else if(cell[1] == '-') device = DV_DECREMENTOR, value = 1; break;
			case '^': if('0' <= cell[1] && cell[1] <= '7') device = DV_BIT_CHECKER; break;
			case '~': if(cell[1] == '~') device = DV_BINARY_NOT; break;
			case ']': if(cell[1] == ']') device = DV_STDIN; break;
			case '}': if(_is_base36(cell[1])) device = DV_INPUT; break;
			case '{': if(_is_base36(cell[1])) device = DV_OUTPUT;
					  else if(cell[1] == '<') device = DV_OUTPUT, value = 255;
					  else if(cell[1] == '>') device = DV_OUTPUT, value = 254; break;
			case '!': if(cell[1] == '!') device = DV_TERMINATOR; break;
			case '?': if(_is_base36(cell[1])) device = DV_RANDOM; 
					  else if(cell[1] == '?') device = DV_RANDOM, value = 253; break;
		}
		if(device == DV_INPUT){
			board.inputs[value].push_front(pos);
		}else if(device == DV_OUTPUT){
			if(value == 255) board.output_left.push_front(pos);
			else if(value == 254) board.output_right.push_front(pos);
			else board.outputs[value].push_front(pos);
		}else if(device == DV_SYNCHRONISER){
			board.synchronisers[value].push_front(pos);
		}else if(device == DV_PORTAL){
			board.portals[value].push_back(pos);
		}
		board.cells[pos] = Cell(device, value);
	}
}
static inline bool _load_board(const std::list<SourceLine> &lines,
							   Board &board){
	// first get dimensions of board..
	board.height = lines.size();
	for(const auto &line : lines){
		std::string sline = line.get_stripped();
		if(!line.is_spaced()){
			// length must be multiple of two
			if(sline.length() % 2){
				line.emit_error("Unexpected character", sline.length() - 1);
				return false;
			}
			board.width = std::max<uint16_t>(board.width, sline.length() / 2);
		}else {
			// spaced format; width is (length - 1)/3 
			// make sure properly formatted..
			for(unsigned i = 0; i < sline.length(); ++i)
				if(i % 3 == 2 && sline[i] != ' '){
					line.emit_error("Expecting space", i);
					return false;
				}else if(i % 3 != 2 && sline[i] == ' '){
					line.emit_error("Not expecting space", i);
				}
			board.width = std::max<uint16_t>(board.width, (sline.length() + 2) / 3);
		}
	}
	// resize cells
	board.cells.resize(board.width * board.height);
	// load cells
	int32_t y = 0;
	for(const auto &line : lines){
		std::string sline = line.get_stripped();
		int32_t x, length = sline.length();
		if(!line.is_spaced()){
			// unspaced format
			for(x = 0; 2 * x < length; ++x){
				_process_cell(line.get_cell_text(x), board.index(x, y), board);
			}
		}else{
			// spaced format
			for(x = 0; 3 * x - 1 < length; ++x){
				_process_cell(line.get_cell_text(x), board.index(x, y), board);
			}
		}
		for(; x < board.width; ++x){
			board.cells[board.index(x, y)] = Cell(DV_BLANK, 0);
		}
		++y;	
	}
	// reverse lists (since push_front rather than push_back was used)
	// this allows for typical left-right top-bottom execution order
	for(int i = 0; i < 36; ++i){
		board.inputs[i].reverse();
		board.outputs[i].reverse();
		board.synchronisers[i].reverse();
	}
	board.output_left.reverse();
	board.output_right.reverse();
	// initialize
	board.initialize();
	return true;
}
static inline bool _load_boards(std::list<SourceLine> &lines, 
								std::vector<Board> &boards, // global
								std::map<std::string, unsigned> &self_ids, // lookup for this file's boards
								std::map<std::string, unsigned> &include_ids, // lookup for included file's boards
								std::map<unsigned, std::list<SourceLine>> &sources, // sources to process
								std::string filename
								){
	unsigned id = boards.size();
	std::list<SourceLine> cur_board_lines;
	bool is_in_board = true;
	// create new board (MB)
	boards.resize(boards.size() + 1);
	boards[id].full_name = _make_full_name(filename, 0, "MB");
	boards[id].short_name = "MB";

	for(auto itr = lines.begin(); itr != lines.end(); ++itr){
		if(is_in_board && (itr->is_include() || (itr->get_source()[0] == ':'))){
			// end current board..
			if(!_load_board(cur_board_lines, boards[id]))
				return false;
			sources[id] = cur_board_lines;
			self_ids[boards[id].actual_name] = id;
			cur_board_lines.clear();
			for(const auto &entry : include_ids){
				if(_names_equivalent(boards[id].short_name, entry.first)){
					include_ids.erase(entry.first);
					break;
				}
			}
			is_in_board = false;
		}
		if(itr->is_include()){
			// load included file
			std::list<SourceLine> included_file;
			std::string file = itr->get_source().substr(include_prefix.size());
			trim_both(file);

			std::map<std::string, unsigned> temp_lookup;
			// load mbl file
			if(!load_mbl_file(file, boards, temp_lookup))
				return false;
			// filter out MB; replace duplicates
			for(const auto entry : temp_lookup){
				if(!_names_equivalent("MB", entry.first)){
					for(const auto &entry1 : self_ids){
						if(_names_equivalent(entry.first, entry1.first)){
							sources.erase(self_ids[entry1.first]);
							self_ids.erase(entry1.first);
							break;
						}
					}
					include_ids[entry.first] = entry.second;
				}
			}
		}else if(itr->get_source()[0] == ':'){
			id = boards.size();
			boards.resize(boards.size() + 1);
			std::string short_name = itr->get_stripped().substr(1);
			if(short_name == ""){
				itr->emit_error("Unnamed board declaration forbidden", 1);
				return false;
			}
			boards[id].short_name = short_name;
			boards[id].full_name = _make_full_name(filename, itr->get_line_number(), short_name);
			is_in_board = true;
		}else if(is_in_board){
			std::string trimmed = itr->get_stripped();
			trim_left(trimmed);
			if(trimmed != "")
				cur_board_lines.push_back(*itr);
		}
	}
	// end last board
	if(is_in_board){
		if(!_load_board(cur_board_lines, boards[id]))
			return false;
		sources[id] = cur_board_lines;
		self_ids[boards[id].actual_name] = id;
		cur_board_lines.clear();
		for(const auto &entry : include_ids){
			if(_names_equivalent(boards[id].short_name, entry.first)){
				include_ids.erase(entry.first);
				break;
			}
		}
	}
	
	return true;
}

static inline bool _resolve_board_calls(std::vector<Board> &boards,
										const std::map<unsigned, std::list<SourceLine>> &board_sources,
										const std::map<std::string, unsigned> &lookup,
										const std::map<std::string, unsigned> &include_lookup
										){
	for(const auto &board_info : lookup){
		const std::list<SourceLine> &source = board_sources.at(board_info.second);
		Board &board = boards[board_info.second];
		std::list<SourceLine>::const_iterator src_itr = source.begin();
		for(uint16_t y = 0; y < board.height; ++y, ++src_itr){
			uint16_t x = 0;
			int32_t start = -1;
			// collect consecutive DV_BOARDs into call_text
			std::string call_text = "";
			while(x <= board.width){
				Device type = DV_BLANK;
				if(x != board.width) type = board.cells[board.index(x, y)].device;
				if((x == board.width || type != DV_BOARD) && start != -1){
					// consecutive boards ended at last cell..
					while(call_text != ""){
						std::string best_match = "";
						unsigned best_match_id;
						// get best match
						for(const auto &entry : lookup){
							if(entry.first.size() > best_match.size()
							&& !call_text.compare(0, entry.first.size(), entry.first)){
								best_match = entry.first;
								best_match_id = entry.second;
							}
						}
						for(const auto &entry : include_lookup){
							if(entry.first.size() > best_match.size()
							&& !call_text.compare(0, entry.first.size(), entry.first)){
								best_match = entry.first;
								best_match_id = entry.second;
							}
						}
						if(best_match != ""){
							// create boardcall
							call_text = call_text.substr(best_match.length());
							board.board_calls.push_front(BoardCall(&boards[best_match_id], start, y));
							// assign board calls
							uint16_t end = start + best_match.size()/2;
							while(start < end){
								board.cells[board.index(start++, y)].board_call = &board.board_calls.front();
							}
						}else{
							// no match found!
							src_itr->emit_error("No board found", (src_itr->is_spaced() ? 3 : 2) * start);
							return false;
						}
					}
				}else if(type == DV_BOARD){
					if(start == -1)
						start = x;
					call_text += src_itr->get_cell_text(x);
				}
				++x;
			}
		}
		// reverse boardcalls list
		// this allows for typical left-right top-bottom execution order
		board.board_calls.reverse();
	}
	return true;
}
bool load_mbl_file(std::string file,
				   std::vector<Board> &boards,
				   std::map<std::string, unsigned> &lookup
				  ){
	std::list<SourceLine> source;
	std::map<std::string, unsigned> include_lookup;
	std::map<unsigned, std::list<SourceLine>> board_sources;

	if(!_load_file(file, source)) return false;
	if(!_strip_blank_lines(source)) return false;
	if(!_load_boards(source, boards, lookup, include_lookup, board_sources, file)) return false;
	if(!_resolve_board_calls(boards, board_sources, lookup, include_lookup)) return false;

	return true;
}
