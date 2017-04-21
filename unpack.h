namespace {
const HuffTableEntry* find_code(uint32_t code)
{
    const HuffTableEntry* const start = huff_decode_table;
    const HuffTableEntry* const end = start + 256;
    const HuffTableEntry* res = std::lower_bound(start, end, code);
    if (res->msb_code > code) {
        assert(res > start);
        res--;
    }
    return res;
}

string decode_huffman(string input)
{
    unsigned min_bits = 30;
    unsigned bits = 0;
    uint64_t n = 0;
    const uint8_t *pos = (const uint8_t*)input.c_str();
    const uint8_t *const end = pos + input.length();
    string res;

    // Hurr. Durr.
    while (bits || pos < end) {
        while (bits < min_bits) {
            uint8_t b1 = 0xff;
            if (pos < end) {
                b1 = *pos++;
            }
            assert(bits + 8 <= 64);
            bits += 8;
            n |= (uint64_t)b1 << (64 - bits);
            //debug("read %#x, have %u bits in %#lx\n", b1, bits, n);
        }
        uint32_t code = n >> 32;
        // EOS
        if (code >= 0xfffffffc) {
            break;
        }
        //debug("have %u bits in %#lx (%#x)\n", bits, n, code);
        const HuffTableEntry* p = find_code(n >> 32);
        assert(p);
        unsigned len = huff_lengths[p->value];
        uint32_t shifted_code = code >> (32 - len);
        debug("found code %#x (%u bits, %#x, %#x), char %#x '%c'\n", p->msb_code, len, p->msb_code >> (32 - len), shifted_code, p->value, p->value);
        assert(shifted_code == (p->msb_code >> (32 - len)));
        n <<= len;
        bits -= len;
        res += (char)p->value;
    }

    debug("huffman-decoded: %s\n", res.c_str());

    return res;
}

unsigned mask(unsigned bits)
{
    return (1 << bits) - 1;
}

unsigned get_int(uint8_t byte1, unsigned b1_mask, const uint8_t*& pos, const uint8_t* const end)
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

string get_string(const uint8_t*& pos, const uint8_t* const end)
{
    assert(pos < end);
    uint8_t b1 = *pos++;
    unsigned length = get_int(b1, mask(7), pos, end);
    const char *start = (const char*)pos;
    pos += length;
    assert(pos <= end);
    string s(start, (const char*)pos);
    if (b1 & 0x80) {
        return decode_huffman(s);
    } else {
        debug("plain string: %s\n", s.c_str());
        return s;
    }
}

class UnpackState {
    string buffer;
    DynamicTable dyn_table;
    unsigned max_dynamic_size;
    string name, value;

public:
    UnpackState(): max_dynamic_size(4096) {}

    void feed(const string& data) {
        buffer += data;
    }

    template <typename T>
    void unpack(T&& callback) {
        const uint8_t *pos = (const uint8_t*)buffer.c_str();
        const uint8_t *const input_end = pos + buffer.length();
        while (pos < input_end) {
            bool push = true;
            int name_ix = 0;
            int both_ix = 0;

            uint8_t b1 = *pos++;
            if (b1 & 0x80) {
                both_ix = get_int(b1, mask(7), pos, input_end);
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

            if (both_ix) {
                const TableEntry& e = dyn_table.get(both_ix);
                name = e.name;
                value = e.value;
            } else if (name_ix) {
                name = dyn_table.get_name(name_ix);
                value = get_string(pos, input_end);
            } else {
                name = get_string(pos, input_end);
                value = get_string(pos, input_end);
            }

            callback(name, value);

            if (push) {
                dyn_table.push(name, value);
                // dyn_table.shrink(max_dynamic_size);
                debug("adding to dyn table: %s = %s (size = %u)\n",
                        name.c_str(), value.c_str(), dyn_table.size);
            }
        }
        assert(pos == input_end);
        buffer.clear();
    }

    void eof() {
        assert(buffer.empty());
    }

    bool has_buffer() const {
        return buffer.size() > 0;
    }
};

} // namespace

