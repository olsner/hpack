CFLAGS = -Wall -pedantic -O2
CXXFLAGS = $(CFLAGS) -std=c++11
LIBS = -lz

all: hpack zpipe

hpack: hpack.cc constants.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@
