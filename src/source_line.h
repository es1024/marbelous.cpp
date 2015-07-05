#ifndef SOURCE_LINE_H
#define SOURCE_LINE_H

#include <string>

class SourceLine{
	public:
		SourceLine() = default;
		SourceLine(std::string file_name, unsigned line_number, std::string source);

		std::string get_source() const;
		// returns string without comments, and right-trimmed
		std::string get_stripped() const;

		bool is_include() const;

		void emit_warning(std::string text, unsigned charPos) const;
		void emit_error(std::string text, unsigned charPos) const;

		std::string get_file_name() const;
		unsigned get_line_number() const;

		bool is_spaced() const;

		std::string get_cell_text(uint16_t cell) const;
	private:
		std::string file_name;
		unsigned line_number;
		std::string source;
		bool spaced;
};

#endif
