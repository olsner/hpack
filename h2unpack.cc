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

#define check_pseudoheaders 0
#define check_end_stream 0
#define save_stream_headers 0

struct Http2State
{
#if check_pseudoheaders
    set<uint32_t> regular_headers_seen;
#endif
#if check_end_stream
    set<uint32_t> end_stream_seen;
#endif
#if save_stream_headers
    typedef vector<pair<string,string>> Headers;
    map<uint32_t, Headers> stream_headers;
#endif
    UnpackState state;

    enum FrameType {
        HEADERS = 1,
        PUSH_PROMISE = 5,
        CONTINUATION = 9,
    };
    enum HeadersFlags {
        END_STREAM = 1,
        END_HEADERS = 4,
        PADDED = 8,
        PRIORITY = 0x20,
    };

    void handle_headers(uint32_t stream, const string& payload) {
#if check_end_stream
        if (end_stream_seen.count(stream)) {
            fprintf(stderr, "Error: Additional HEADERS after end_stream for %u\n",
                    stream);
        }
#endif
        state.feed(payload);
#if save_stream_headers
        Headers& headers = stream_headers[stream];
#endif
        state.unpack([&](const string& name, const string& value) {
#if check_pseudoheaders
            if (name[0] == ':') {
                if (regular_headers_seen.count(stream)) {
                    fprintf(stderr, "Error: Pseudo-header %s follows regular headers for stream %u\n", name.c_str(), stream);
                }
            } else {
                regular_headers_seen.insert(stream);
            }
#endif
#if save_stream_headers
            headers.push_back({name, value });
#endif
            fprintf(stderr, "%u: %s: %s\n", stream, name.c_str(), value.c_str());
        });
    }

    void read_frame(string& input) {
        size_t size = read_u24(input);
        uint8_t type = read_u8(input);
        uint8_t flags = read_u8(input);
        uint32_t stream = read_u32(input);

        fprintf(stderr, "stream=%u, type=%d, %zu bytes [%02x %02x %02x] [%02x %02x %02x]\n",
                stream, type, size,
                input[0] & 0xff, input[1] & 0xff, input[2] & 0xff,
                input[size + 0] & 0xff, input[size + 1] & 0xff, input[size + 2] & 0xff);
        assert(size <= 16384);

        string payload = input.substr(0, size);
        input.erase(0, size);

        assert(!(flags & PADDED));
        switch (type) {
        case HEADERS:
            {
                assert(flags & END_HEADERS);
                if (flags & PRIORITY) {
                    /*uint8_t weight =*/ read_u8(payload);
                }

                handle_headers(stream, payload);
            }
            break;
        case PUSH_PROMISE:
            {
                assert(flags & END_HEADERS);
                uint32_t promised = read_u32(payload);
                handle_headers(promised, payload);
                break;
            }
        case CONTINUATION:
            assert(!"Not handled");
        case 0:
            {
                //            asm("int3");
            }
        }
#if check_end_stream
        if (flags & END_STREAM) {
            end_stream_seen.insert(stream);
        }
#endif
    }

    void eof() {
        state.eof();
    }

};

}

int main(int argc, const char *argv[])
{
    Http2State state;
    string input = read_fully(stdin);
    const size_t orig_size = input.size();
    while (input.size()) {
        size_t pos = orig_size - input.size();
        state.read_frame(input);
    }
    state.eof();
}
