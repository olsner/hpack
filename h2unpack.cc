#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "unpack.h"

namespace {

uint8_t read_u8(string& input) {
    uint8_t res = input[0];
    input.erase(0, 1);
    return res;
}
uint16_t read_u16(string& input) {
    uint32_t res = read_u8(input);
    return (res << 8) | read_u8(input);
}
uint32_t read_u24(string& input) {
    uint32_t res = read_u16(input);
    return (res << 8) | read_u8(input);
}
uint32_t read_u32(string& input) {
    uint32_t res = read_u16(input);
    return (res << 16) | read_u16(input);
}

set<uint32_t> regular_headers_seen;
set<uint32_t> end_stream_seen;
typedef vector<pair<string,string>> Headers;
map<uint32_t, Headers> stream_headers;

enum FrameType {
    HEADERS = 1,
    PUSH_PROMISE = 5,
};
enum HeadersFlags {
    END_STREAM = 1,
    END_HEADERS = 4,
    PADDED = 8,
    PRIORITY = 0x20,
};

void handle_headers(UnpackState& state, uint32_t stream, const string& payload) {
    if (end_stream_seen.count(stream)) {
        fprintf(stderr, "Error: Additional HEADERS after end_stream for %u\n",
                stream);
    }
    state.feed(payload);
    Headers& headers = stream_headers[stream];
    state.unpack([&](const string& name, const string& value) {
        if (name[0] == ':') {
            if (regular_headers_seen.count(stream)) {
                fprintf(stderr, "Error: Pseudo-header %s follows regular headers for stream %u\n", name.c_str(), stream);
            }
        } else {
            regular_headers_seen.insert(stream);
        }
        headers.push_back({name, value });
        fprintf(stderr, "%u: %s: %s\n", stream, name.c_str(), value.c_str());
    });
}

void read_frame(string& input, UnpackState& state) {
    size_t size = read_u24(input);
    assert(size <= 16384);
    uint8_t type = read_u8(input);
    uint8_t flags = read_u8(input);
    uint32_t stream = read_u32(input);

    string payload = input.substr(0, size);
    input.erase(0, size);

    fprintf(stderr, "stream=%u, type=%d, %zu bytes [%02x %02x %02x] [%02x]\n",
            stream, type, size,
            payload[0] & 0xff, payload[1] & 0xff, payload[2] & 0xff,
            input[0] & 0xff);
    assert(!(flags & PADDED));
    switch (type) {
    case HEADERS:
        {
            assert(flags & END_HEADERS);
            if (flags & PRIORITY) {
                /*uint8_t weight =*/ read_u8(payload);
            }

            handle_headers(state, stream, payload);
        }
        break;
    case PUSH_PROMISE:
        {
            assert(flags & END_HEADERS);
            handle_headers(state, stream, payload);
            break;
        }
    }
    if (flags & END_STREAM) {
        end_stream_seen.insert(stream);
    }
}

}

int main(int argc, const char *argv[])
{
    UnpackState state;
    string input = read_fully(stdin);
    while (input.size()) {
        read_frame(input, state);
    }
    state.eof();
}
