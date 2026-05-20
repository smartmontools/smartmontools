# Makefile — build and run snmp-agentxd unit + integration tests
# No net-snmp required.  Run: make test FIXTURES=<path-to-fixture-dir>

CXX      ?= g++
CXXFLAGS ?= -std=c++14 -Wall -Wextra -g
SRCDIR   := ..
FIXTURES ?= $(abspath ../fixtures)

# Source files shared by tests (no net-snmp dependencies)
COMMON_SRC := $(SRCDIR)/agentxd_json.cpp $(SRCDIR)/agentxd_cache.cpp

TESTS := test_json test_datasrc

all: $(TESTS)

test_json: test_json.cpp $(SRCDIR)/agentxd_json.cpp
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $@ $^

DATASRC_DEPS := $(SRCDIR)/agentxd_datasrc.cpp $(SRCDIR)/agentxd_datasrc.h \
                $(SRCDIR)/agentxd_cache.cpp $(SRCDIR)/agentxd_cache.h \
                $(SRCDIR)/agentxd_json.cpp  $(SRCDIR)/agentxd_json.h \
                $(SRCDIR)/agentxd_config.h

test_datasrc: test_datasrc.cpp $(DATASRC_DEPS)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $@ $<

# Run all tests; pass fixture files if the directory exists
FIXTURE_FILES := $(wildcard $(FIXTURES)/*.json)

test: all
	@echo "=== test_json ==="
	./test_json $(FIXTURE_FILES)
	@echo "=== test_datasrc ==="
	./test_datasrc $(FIXTURE_FILES)
	@echo "=== All tests passed ==="

clean:
	rm -f $(TESTS)

.PHONY: all test clean
