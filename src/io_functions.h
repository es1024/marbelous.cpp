#ifndef IO_FUNCTIONS_H
#define IO_FUNCTIONS_H

#include <cstdint>
#include <vector>

// init/cleans up io (init = false for cleanup)
void prepare_io(bool init);
// changeable function pointers..
extern bool (*stdin_available)(void);
extern uint8_t (*stdin_get)(void);
extern void (*stdout_write)(uint8_t);
// check if any char is ready to be read on stdin
bool _stdin_available();
// get character from stdin
uint8_t _stdin_get();
// output character to stdout
void _stdout_write(uint8_t value);
// save stdout character instead of writing
void _stdout_save(uint8_t value);
// output character to stdout as hex
void _stdout_writehex(uint8_t value);

// get saved stdout characters
const std::vector<uint8_t> &stdout_get_saved();

#endif // IO_FUNCTIONS_H
