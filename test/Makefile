CXX ?= g++
CXXFLAGS ?= -std=c++14 -Wall -Wextra -Werror -I..
SANITIZER_FLAGS ?= -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: test clean

test: test_http_parser test_etherport
	./test_http_parser
	ASAN_OPTIONS=detect_leaks=0 ./test_etherport

test_http_parser: test_http_parser.cpp ../HttpParser.cpp ../HttpParser.h
	$(CXX) $(CXXFLAGS) test_http_parser.cpp ../HttpParser.cpp -o $@

test_etherport: test_etherport.cpp ../etherport.cpp ../etherport.h
	$(CXX) $(CXXFLAGS) $(SANITIZER_FLAGS) test_etherport.cpp ../etherport.cpp \
		-lssl -lcrypto -o $@

clean:
	rm -f test_http_parser test_etherport
