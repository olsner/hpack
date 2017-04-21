#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "unpack.h"

int main(int argc, const char *argv[])
{
    UnpackState state;
    state.feed(read_fully(stdin));
    state.unpack([](const string& name, const string& value) {
        printf("%s: %s\n", name.c_str(), value.c_str());
        debug("=> %s: %s\n", name.c_str(), value.c_str());
    });
    state.eof();
}
