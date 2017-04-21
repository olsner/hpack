#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nghttp2/nghttp2.h>

#include "common.h"

#define error(...) do { fprintf(stderr, __VA_ARGS__); abort(); } while (0)

int main(int argc, const char *argv[])
{
    vector<nghttp2_nv> header_list;

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

        header_list.push_back({});
        header_list.back().name = (uint8_t*)strdup(name.c_str());
        header_list.back().namelen = name.size();
        header_list.back().value = (uint8_t*)strdup(value.c_str());
        header_list.back().valuelen = value.size();
    }

    nghttp2_hd_deflater *deflater = nullptr;
    if (int res = nghttp2_hd_deflate_new(&deflater, NGHTTP2_DEFAULT_HEADER_TABLE_SIZE)) {
        error("Error creating deflater: %s\n", nghttp2_strerror(res));
    }

    size_t bufsize = nghttp2_hd_deflate_bound(deflater,
            &header_list[0], header_list.size());

    uint8_t* buf = (uint8_t*)malloc(bufsize);

    // nghttp2_hd_deflate_hd claims to return a size_t, but the value
    // can be negative for errors
    ssize_t dlen = nghttp2_hd_deflate_hd(deflater,
            buf, bufsize,
            &header_list[0], header_list.size());
    while (header_list.size()) {
        free(header_list.back().name);
        free(header_list.back().value);
        header_list.pop_back();
    }
    if (dlen < 0)
    {
        free(buf);
        error("Error deflating headers: %s\n", nghttp2_strerror(dlen));
    }

    fwrite((const char*)buf, 1, dlen, stdout);
    free(buf);
    nghttp2_hd_deflate_del(deflater);
}
