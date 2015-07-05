#include "io_functions.h"

#include <cstdio>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
	// unix-based systems: use pollfd, poll
	#define UNIX 1
	#include <sys/poll.h>
#elif defined(_WIN32)
	// windows systems: use _kbhit
	#include <Windows.h>
	#include <conio.h>
#else
	#error "I/O for your OS is not supported, please see src/io_functions.cpp"
#endif

// init = true for init, init = false for restore to original
void prepare_io(bool init){
	#if defined(UNIX)
		static struct termios old_tio;
		if(init){
			// disable line input buffering
			struct termios new_tio;
			tcgetattr(STDIN_FILENO, &old_tio);

			new_tio = old_tio;
			new_tio.c_lflag &= ~ICANON;
			tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
		}else{
			// restore settings
			tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
		}
	#elif defined(_WIN32)
		static DWORD mode;
		auto stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
		if(init){
			// diable line input buffering
			GetConsoleMode(stdin_handle, &mode);
			SetConsoleMode(stdin_handle, mode & ~ENABLE_LINE_INPUT);
		}else{
			// restore settings
			SetConsoleMode(stdin_handle, mode);
		}
	#endif
}

// check if there is something to be read on stdin
bool stdin_available(){
	#if defined(UNIX)
		struct pollfd fds;
		fds.fd = 0; // stdin
		fds.event = POLLIN;
		return poll(&fds, 1, 0);
	#elif defined(_WIN32) 
		return _kbhit();
	#endif
}

uint8_t stdin_get(){
	int ch;
	#if defined(UNIX)
		ch = getchar();
	#elif defined(_WIN32)
		ch = _getch();
	#endif
	return static_cast<uint8_t>(ch);
}

void stdout_write(uint8_t value){
	putchar(value);
	// std::printf("Output: %c (%d)\n", value, value);
}
