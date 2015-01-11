#!/usr/bin/env python

import sys

sys.path.append("hyper")
import hyper

codes = hyper.http20.huffman_constants.REQUEST_CODES
lengths = hyper.http20.huffman_constants.REQUEST_CODES_LENGTH

table = []
for i,code in enumerate(hyper.http20.huffman_constants.REQUEST_CODES):
    table.append((code << (32 - lengths[i]), i))
table.sort()

for code,i in table:
    print "{ 0x%x, 0x%x }," % (code, i)
