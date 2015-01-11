CFLAGS = -Wall -pedantic -O2 -g
CXXFLAGS = $(CFLAGS) -std=c++11
LIBS = -lz

all: hpack hunpack zpipe spdy3_putdict

%: %.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@
%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

hpack: hpack.cc common.h
hunpack: hunpack.cc common.h
