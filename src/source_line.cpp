#include "emit.h"
#include "source_line.h"
#include "trim.h"

#include <algorithm>
#include <utility>

const std::string include_prefix = "#include";

SourceLine::SourceLine(std::string file_name, unsigned line_number, std::string source):
	file_name(file_name),
	line_number(line_number),
	source(source){
	// spaced format must have at least 1 isolated single space
	spaced = source.find("  ") == std::string::npos && source.find(" ") != std::string::npos;
}

std::string SourceLine::get_source() const {
	return source;
}

std::string SourceLine::get_stripped() const {
	std::string sourceCopy = source;
	sourceCopy = sourceCopy.substr(0, sourceCopy.find('#'));
	return trim_right(sourceCopy);
}

bool SourceLine::is_include() const {
	return !source.compare(0, include_prefix.length(), include_prefix);
}

void SourceLine::emit_warning(std::string text, unsigned charPos) const {
	unsigned displacement = charPos > 30 ? charPos - 30 : 0;
	::emit_warning(file_name + ":" + std::to_string(line_number) + ":" + 
		std::to_string(charPos) + ": " + text);
	::emit_warning(source.substr(displacement, 60));
	::emit_warning(std::string(' ', charPos - displacement) + "^");
}

void SourceLine::emit_error(std::string text, unsigned charPos) const {
	unsigned displacement = charPos > 30 ? charPos - 30 : 0;
	::emit_error(file_name + ":" + std::to_string(line_number) + ":" + 
		std::to_string(charPos) + ": " + text);
	::emit_error(source.substr(displacement, 60));
	::emit_error(std::string(charPos - displacement, ' ') + "^");
}

std::string SourceLine::get_file_name() const {
	return file_name;
}
unsigned SourceLine::get_line_number() const {
	return line_number;
}

bool SourceLine::is_spaced() const {
	return spaced;
}

std::string SourceLine::get_cell_text(uint16_t cell) const {
	if(spaced) return source.substr(3 * cell, 2);
	else return source.substr(2 * cell, 2);
}
