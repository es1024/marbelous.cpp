
#ifndef FOR_EACH_CELL_TYPE
// call(CELL_TYPE_NAME)
#define FOR_EACH_CELL_TYPE(call) \
	/* Flow Control */ \
	call(LEFT_DEFLECTOR) \
	call(RIGHT_DEFLECTOR) \
	call(PORTAL) \
	call(SYNCHRONISER) \
	/* Conditionals */ \
	call(EQUALS) \
	call(GREATER_THAN) \
	call(LESS_THAN) \
	/* Arithmetic */ \
	call(ADDER) \
	call(SUBTRACTOR) \
	call(INCREMENTOR) \
	call(DECREMENTOR) \
	/* Bit Operations */ \
	call(BIT_CHECKER) \
	call(LEFT_BIT_SHIFTER) \
	call(RIGHT_BIT_SHIFTER) \
	call(BINARY_NOT) \
	/* Input/Output */ \
	call(STDIN) \
	call(INPUT) \
	call(OUTPUT) \
	/* Miscellaneous Devices */ \
	call(TRASH_BIN) \
	call(CLONER) \
	call(TERMINATOR) \
	call(RANDOM) \
	/* Non-device cell types */ \
	call(BLANK) \
	call(BOARD)

#endif
