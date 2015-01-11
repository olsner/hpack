#!/usr/bin/env python

import json
import os
import subprocess
import sys
import time

sys.path.append("hyper")
import hyper

tests_dirs = "hpack-test-case"
tests_dir = "hpack-test-case/raw-data"
total_size = 0
total_orig = 0
total_best = 0

def decode_hpack(data):
    d = hyper.http20.hpack.Decoder()
    def f(kv):
        return "%s: %s\n" % kv
    return ''.join(map(f, d.decode(data)))

def get_wires(cases):
    res = []
    for case in cases:
        wire = case.get('wire')
        if wire is None: return None
        res.append(wire.decode('hex'))
    return res

def find_best(f):
    min_len = None
    best_data = None
    best_name = None
    for d in os.listdir(tests_dirs):
        p = os.path.join(tests_dirs, d, f)
        if not os.path.isfile(p) or d == 'raw-data':
            continue
        with open(p, "r") as h:
            data = json.load(h)
        refdata = get_wires(data['cases'])
        if refdata is None:
            print "no data for", f, "in", d
            continue
        refdata = ''.join(refdata)
        if min_len is None or min_len > len(refdata):
            min_len = len(refdata)
            best_data = refdata
            best_name = d
    return best_data, best_name

def parse_test(f):
    with open(os.path.join(tests_dir, f), "r") as h: data = json.load(h)
    cases = data['cases']
    headers = []
    size = 0
    for case in cases:
        for kv in case['headers']:
            for k,v in kv.iteritems():
                assert k == k.lower(), k
                headers.append("%s: %s\n" % (k,v))
                size += len(k) + len(v)
        table_size = int(case.get('header_table_size', 4096))
        if table_size != 4096:
            print "WARNING:", f, "has custom table size:", table_size
    refdata, refname = find_best(f)
    return f, ''.join(headers), size, refdata, refname

def run_test(f, headers, orig_size, refdata, refname):
    proc = subprocess.Popen(["./hpack"], stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    t = time.time()
    res,err = proc.communicate(headers)
    t = time.time() - t
    pct = 100.0 * len(res) / orig_size
    print f, len(res), "%.3fms" % (t * 1000), "%.0f%%, %s %+d" % (pct, refname, len(res) - len(refdata))
    assert headers == decode_hpack(res)
    global total_size, total_orig, total_best
    total_size += len(res)
    total_orig += orig_size
    total_best += len(refdata)

tests = []
for f in sorted(os.listdir(tests_dir)):
    if not f.endswith(".json"):
        continue
    tests.append(parse_test(f))
for test in tests:
    run_test(*test)

pct = 100.0 * total_size / total_orig
print "total %d/%d (%.0f%%)" % (total_size, total_orig, pct)
pct_ng = 100.0 * total_best / total_size - 100
print "combo-best %d/%d (%+.1f%%)" % (total_best, total_size, pct_ng)
