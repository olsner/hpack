CFLAGS = -Wall -pedantic -O2 -g
CXXFLAGS = $(CFLAGS) -std=c++11
LIBS = -lz

all: hpack zpipe spdy3_putdict

hpack: hpack.cc constants.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@
