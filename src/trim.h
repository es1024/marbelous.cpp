#ifndef TRIM_H
#define TRIM_H

#include <algorithm>
#include <utility>

inline std::string &trim_left(std::string &str){
	str.erase(str.begin(),
		std::find_if(
			str.begin(),
			str.end(),
			std::not1(std::ptr_fun<int, int>(std::isspace))
		));
	return str;
}
inline std::string &trim_right(std::string &str){
	str.erase(std::find_if(
		str.rbegin(),
		str.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))
	).base(), str.end());
	return str;
}
inline std::string &trim_both(std::string &str){
	return trim_right(trim_left(str));
}

#endif
