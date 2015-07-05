#include "emit.h"

#include <cstdio>

void emit_error(std::string text){
	std::fputs(text.c_str(), stderr);
	std::fputc('\n', stderr);
}

void emit_warning(std::string text){
	std::fputs(text.c_str(), stdout);
	std::fputc('\n', stdout);
}
