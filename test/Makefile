CXX ?= g++
CXXFLAGS ?= -std=c++14 -Wall -Wextra -Werror -I..

.PHONY: test clean

test: test_http_parser
	./test_http_parser

test_http_parser: test_http_parser.cpp ../HttpParser.cpp ../HttpParser.h
	$(CXX) $(CXXFLAGS) test_http_parser.cpp ../HttpParser.cpp -o $@

clean:
	rm -f test_http_parser
