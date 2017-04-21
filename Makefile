NGHTTP2 ?= ../nghttp2
CFLAGS = -Wall -pedantic -O2 -g -MD -MP -I$(NGHTTP2)/lib/includes
CXXFLAGS = $(CFLAGS) -std=c++11
LIBS = -lz $(NGHTTP2)/lib/.libs/libnghttp2.a

BINARIES = hpack hunpack zpipe spdy3_putdict ng_hpack
all: $(BINARIES)

%: %.cc
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $< $(LIBS)
%: %.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $< $(LIBS)

-include $(BINARIES:%=%.d)

clean:
	rm -f $(BINARIES)
