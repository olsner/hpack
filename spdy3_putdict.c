#include <unistd.h>

#include "spdy3.h"

int main() {
	return write(1, SPDY3_dict, sizeof(SPDY3_dict)) != sizeof(SPDY3_dict);
}
