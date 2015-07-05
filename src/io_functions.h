#ifndef IO_FUNCTIONS_H
#define IO_FUNCTIONS_H

#include <cstdint>

// init/cleans up io (init = false for cleanup)
void prepare_io(bool init);
// check if any char is ready to be read on stdin
bool stdin_available();
// get character from stdin
uint8_t stdin_get();
// output character to stdout
void stdout_write(uint8_t value);

#endif // IO_FUNCTIONS_H
