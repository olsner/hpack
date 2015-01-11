Don't expect much of this - I just wanted to see how hard it could be to make
a HPACK encoder. As it turns out: not very.

On the collection of cases from hpack-test-case, my total size is about 0.3%
larger than nghttp2. If you combine all the other implementations and pick the
best one for each individual test case, my total is about 2% worse.

This small difference might just mean that the competing implementations are
not very good yet, but given the simple format there's probably not a whole lot
more you can do.

## License ##

This is licensed under the MIT license (see LICENSE), except for zpipe.c which
seems to be public domain.

## Instructions ##

Build: run make.
Test: build, check out the submodules, then run ./run_tests.py
Use: if you really want to, hpack takes a series of "header: value" lines on
stdin and writes binary hpack data on stdout.
