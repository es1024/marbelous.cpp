#ifndef OPTIONS_H
#define OPTIONS_H

#include "optionparser.h"

enum Options{
	OPT_UNKNOWN,
	OPT_HELP,
	OPT_VERBOSE,
};

const option::Descriptor usage[] = {
	{OPT_UNKNOWN, 0, "", "", option::Arg::None, "Usage: marbelous [options] file\n"
	                                            "Options: "},
	{OPT_HELP, 0, "", "help", option::Arg::None, "  --help  \tDisplay this information"},
	{OPT_VERBOSE, 0, "v", "", option::Arg::None, "  -v[vv]  \tSet verbosity level, default 0"},
	{0, 0, 0, 0, 0, 0}
};

// defined in main.cpp
extern option::Option *options;

#endif // OPTIONS_H
