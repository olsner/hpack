#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

static unsigned mask(unsigned bits)
{
    return (1 << bits) - 1;
}

static unsigned get_int(uint8_t byte1, unsigned b1_mask, const uint8_t*& pos, const uint8_t* const end)
{
    unsigned val = byte1 & b1_mask;
    if (val < b1_mask) {
        return val;
    }

    unsigned bitpos = 0;
    val = 0;
    while (pos < end) {
        uint8_t b = *pos++;
        val |= (b & 0x7f) << bitpos;
        bitpos += 7;
        if (!(b & 0x80)) {
            break;
        }
    }
    return val + b1_mask;
}

static string get_string(const uint8_t*& pos, const uint8_t* const end)
{
    assert(pos < end);
    uint8_t b1 = *pos++;
    unsigned length = get_int(b1, mask(7), pos, end);
    const char *start = (const char*)pos;
    pos += length;
    if (b1 & 0x80) {
        return "<huffman encoded>";
    } else {
        return string(start, (const char*)pos);
    }
}

int main(int argc, const char *argv[])
{
    unsigned max_dynamic_size = 4096;
    DynamicTable dyn_table;

    string input = read_fully(stdin);
    const uint8_t *pos = (const uint8_t*)input.c_str();
    const uint8_t *const input_end = pos + input.length();
    while (pos < input_end) {
        bool push = true;
        int name_ix = 0;
        int value_ix = 0;

        uint8_t b1 = *pos++;
        if (b1 & 0x80) {
            name_ix = value_ix = get_int(b1, mask(7), pos, input_end);
            debug("indexed (both): %d\n", name_ix);
            push = false;
        } else if (b1 & 0x40) {
            name_ix = get_int(b1, mask(6), pos, input_end);
            debug("indexed (name): %d\n", name_ix);
        } else if (b1 & 0x20) {
            max_dynamic_size = get_int(b1, mask(5), pos, input_end);
            debug("changed dynamic table size: %u\n", max_dynamic_size);
            dyn_table.shrink(max_dynamic_size);
            continue;
        } else {
            // 0000xxxx or 0001xxxx, with x = 0 for name not indexed
            // Both are unindexed
            push = false;
            name_ix = get_int(b1, mask(4), pos, input_end);
            debug("unindexed (name): %d\n", name_ix);
        }

        string name, value;
        if (name_ix) {
            const TableEntry& e = dyn_table.get(name_ix);
            name = e.name;
            value = value_ix ? e.value : get_string(pos, input_end);
        } else {
            name = get_string(pos, input_end);
            value = get_string(pos, input_end);
        }

        printf("%s: %s\n", name.c_str(), value.c_str());
        if (push) {
            dyn_table.push(name, value);
            dyn_table.shrink(max_dynamic_size);
            debug("adding to dyn table: %s = %s (size = %u)\n", name.c_str(), value.c_str(), dyn_table.size);
        }
    }
}
