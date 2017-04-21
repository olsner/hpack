#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

namespace {
const bool USE_HUFFMAN = true;

void put8(uint8_t v)
{
    fwrite(&v, 1, 1, stdout);
}

void put_vint(unsigned value)
{
    while (value >= 0x80) {
        put8(0x80 | (value & 0x7f));
        value >>= 7;
    }
    put8(value);
}

void put_int(uint8_t prebyte, unsigned prebits, unsigned value)
{
    const unsigned maxval = (1 << prebits) - 1;
    if (value < maxval) {
        put8(prebyte | value);
    } else {
        put8(prebyte | maxval);
        put_vint(value - maxval);
    }
}

unsigned drain(string& out, unsigned& bits, unsigned len)
{
    while (len >= 8) {
        len -= 8;
        out += (char)(uint8_t)(bits >> len);
        bits &= (1 << len) - 1;
    }
    return len;
}

string huff(const string &h)
{
    unsigned bits = 0;
    unsigned n = 0;

    string out;
    const char *p = h.c_str();
    while (uint8_t c = *p++) {
        uint32_t code = huff_codes[c];
        unsigned len = huff_lengths[c];
        if (len > 24) {
            bits <<= len - 24;
            bits |= (code >> (len - 25));
            n += len - 24;
            len = 24;
            n = drain(out, bits, n);
        }
        bits <<= len;
        bits |= code;
        n = drain(out, bits, n + len);
    }
    if (n) {
        bits <<= 7;
        bits |= 0x7f;
        n = drain(out, bits, n + 7);
    }
    return out;
}

void put_string(const string &s)
{
    if (USE_HUFFMAN) {
        string h = huff(s);
        if (h.size() < s.size()) {
            put_int(0x80, 7, h.size());
            fwrite(h.c_str(), 1, h.length(), stdout);
            return;
        }
    }
    put_int(0, 7, s.size());
    fwrite(s.c_str(), 1, s.length(), stdout);
}
}

int main(int argc, const char *argv[])
{
    const unsigned max_dynamic_size = 4096;
    DynamicTable dyn_table;

    string input = read_fully(stdin);
    const char *pos = input.c_str();
    const char *input_end = pos + input.length();
    while (*pos) {
        const char *start = pos;
        const char *end = strchr(start, '\n');
        pos = end ? end + 1 : input_end;
        const char *name_end = strchr(start + 1, ':');
        const char *value_start = name_end + 1;
        value_start += strspn(value_start, " ");

        string name = string(start, name_end);
        string value = string(value_start, end ? end : input_end);

        debug("\nparsed %s = %s\n", name.c_str(), value.c_str());

        bool push = true;
        size_t table_size = 32 + name.length() + value.length();
        // TODO Add a rule (like similar code in other encoders) for deciding
        // if a header value is sensitive and should be forced literal.
        uint8_t sensitive = 0; // 0x10;
        if (sensitive || table_size > 3 * max_dynamic_size / 4) {
            // Sensitive => "literal header never indexed", intermediaries must
            // not use indexed encoding for this.
            // not sensitive => "literal header without indexing", just avoid
            // blowing away the dynamic table.
            debug("oversized/sensitive (%zu), non-indexed\n", table_size);
            int name_ix = dyn_table.find(name);
            if (name_ix) {
                put_int(sensitive, 4, name_ix);
            } else {
                put8(sensitive);
                put_string(name);
            }
            put_string(value);
            push = false;
        } else if (int i = dyn_table.find(name, value)) {
            debug("index (both): %d\n", i);
            push = false; // References are never added to the table.
            put_int(0x80, 7, i);
        } else if (int i = dyn_table.find(name)) {
            debug("index (name): %d\n", i);
            // put value as literal, may want to decide on indexed or not.
            put_int(0x40, 6, i);
            put_string(value);
        } else {
            debug("literal\n");
            // always indexed, but may want to decide :)
            put8(0x40);
            put_string(name);
            put_string(value);
        }
        if (push) {
            dyn_table.push(name, value);
            dyn_table.shrink(max_dynamic_size);
            debug("adding to dyn table: %s = %s (size = %u)\n", name.c_str(), value.c_str(), dyn_table.size);
        }
    }
}
