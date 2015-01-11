CXXFLAGS = -Wall -pedantic -O2 -std=c++11

hpack: hpack.cc constants.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@
