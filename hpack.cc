#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <deque>
#include <map>
#include <string>

//#define debug(...) fprintf(stderr, ## __VA_ARGS__)
#define debug(...) (void)0

static const bool USE_HUFFMAN = true;

using std::deque;
using std::map;
using std::string;

struct TableEntry
{
    const string name, value;

    TableEntry(string name, string value = ""): name(name), value(value) {}

    unsigned size() const {
        return 32 + name.length() + value.length();
    }
};

#include "constants.h"

template <class It>
static int find(string name, string value, It p, It end, int offset)
{
    for (size_t i = 0; p != end; p++, i++) {
        if (p->name == name && p->value == value) {
            return i + offset;
        }
    }
    return 0;
}

template <class It>
static int find(string name, It p, It end, int offset)
{
    for (size_t i = 0; p != end; p++, i++) {
        if (p->name == name) {
            return i + offset;
        }
    }
    return 0;
}

static int find_static(string name)
{
    return find(name, static_table, static_table + STATIC_TABLE_COUNT, 1);
}
static int find_static(string name, string value)
{
    return find(name, value, static_table, static_table + STATIC_TABLE_COUNT, 1);
}

struct DynamicTable
{
    deque<TableEntry> table;
    unsigned size;

    DynamicTable(): size(0) {}

    void shrink(unsigned max_size) {
        while (size > max_size && table.size()) {
            TableEntry &e = table.back();
            debug("%u bytes over budget, evicting %s = %s for %u bytes\n", size - max_size, e.name.c_str(), e.value.c_str(), e.size());
            size -= e.size();
            table.pop_back();
        }
    }

    int find(string name, string value) {
        if (int i = find_static(name, value)) {
            return i;
        } else {
            return ::find(name, value, table.begin(), table.end(), dynamic_table_start);
        }
    }

    int find(string name) {
        if (int i = find_static(name)) {
            return i;
        } else {
            return ::find(name, table.begin(), table.end(), dynamic_table_start);
        }
    }

    void push(string name, string value) {
        table.push_front(TableEntry(name, value));
        size += table.front().size();
    }
};

static void put8(uint8_t v)
{
    fwrite(&v, 1, 1, stdout);
}

static void put_vint(unsigned value)
{
    while (value >= 0x80) {
        put8(0x80 | (value & 0x7f));
        value >>= 7;
    }
    put8(value);
}

static void put_int(uint8_t prebyte, unsigned prebits, unsigned value)
{
    const unsigned maxval = (1 << prebits) - 1;
    if (value < maxval) {
        put8(prebyte | value);
    } else {
        put8(prebyte | maxval);
        put_vint(value - maxval);
    }
}

static unsigned drain(string& out, unsigned& bits, unsigned len)
{
    while (len >= 8) {
        len -= 8;
        out += (char)(uint8_t)(bits >> len);
        bits &= (1 << len) - 1;
    }
    return len;
}

static string huff(const string &h)
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

static void put_string(const string &s)
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

static string read_fully(FILE *fp)
{
    string t;
    while (!feof(fp)) {
        char tmp[1024];
        size_t n = fread(tmp, 1, sizeof(tmp), fp);
        t.append(tmp, n);
        assert(!ferror(fp));
    }
    return t;
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
        if (int i = dyn_table.find(name, value)) {
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
