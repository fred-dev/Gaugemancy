# Unit tests for the openFrameworks-free core (src/core).
#
#   make -C tests                 build and run
#   make -C tests SANITIZE=1      with AddressSanitizer/UBSan
#
# Needs a C++17 compiler and nlohmann/json. By default the copy bundled with
# openFrameworks is used (OF_ROOT defaults to ../../../.. as for an app in
# apps/myApps/Gaugemancy); override JSON_INCLUDE if it lives elsewhere.

OF_ROOT ?= ../../../..
JSON_INCLUDE ?= $(OF_ROOT)/libs/json/include
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra
ifeq ($(SANITIZE),1)
	CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
endif

CORE_SOURCES := $(wildcard ../src/core/*.cpp)
CORE_HEADERS := $(wildcard ../src/core/*.h)

run: test_core
	./test_core

test_core: test_core.cpp TestFramework.h $(CORE_SOURCES) $(CORE_HEADERS)
	$(CXX) $(CXXFLAGS) -I../src -I$(JSON_INCLUDE) test_core.cpp $(CORE_SOURCES) -o $@

clean:
	rm -f test_core

.PHONY: run clean
