#include "devices.h"

const char *device_names[] = {
	#define X(device_name) #device_name,
		FOR_EACH_DEVICE(X)
	#undef X

	0
};
