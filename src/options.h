#ifndef OPTIONS_H
#define OPTIONS_H

#include "optionparser.h"

enum Options{
	OPT_UNKNOWN,
	OPT_HELP,
	OPT_VERBOSE,
	OPT_CYLINDRICAL,
};

enum OptionsType{
	OPT_TYPE_DISABLE,
	OPT_TYPE_ENABLE,
};
const option::Descriptor usage[] = {
	{OPT_UNKNOWN, 0, "", "", option::Arg::None, "Usage: marbelous [options] file.mbl [arguments]\n"
	                                            "Options: "},
	{OPT_HELP, 0, "", "help", option::Arg::None, "  --help  \tDisplay this information"},
	{OPT_VERBOSE, 0, "v", "", option::Arg::None, "  -v[vv]  \tSet verbosity level, default 0"},
	{OPT_CYLINDRICAL, OPT_TYPE_ENABLE, "", "enable-cylindrical", option::Arg::None, 
	    "  --enable-cylindrical  \tEnable or disable cylindrical boards (default disabled)"},
	{OPT_CYLINDRICAL, OPT_TYPE_DISABLE, "", "disable-cylindrical", option::Arg::None, "  --disable-cylindrical"},
	{0, 0, 0, 0, 0, 0}
};

// defined in main.cpp
extern option::Option *options;

extern int verbosity;
extern bool cylindrical;

#endif // OPTIONS_H
