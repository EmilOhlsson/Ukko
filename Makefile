.PHONY: all debug sanitized release install format clean

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.hpp)
OBJS = $(SRCS:.cpp=.o)

CC ?= g++
CXX ?= g++
DUMMY ?= 1

CXXFLAGS_sanitize = -fsanitize=address -fno-omit-frame-pointer
CXXFLAGS_release = -O3
CXXFLAGS_debug = -g -Og

CXXFLAGS += -Wall -Wextra 
CXXFLAGS += -DDUMMY=$(DUMMY)
CXXFLAGS += -MMD
CXXFLAGS += -std=c++20
CXXFLAGS += $$(pkg-config --cflags cairomm-1.0 lua libcurl)
CXXFLAGS += $(CXXFLAGS_$(PROFILE))

LDLIBS_sanitize += -static-libasan
LDLIBS += -lstdc++ -lfmt -lgpiod $$(pkg-config --libs cairomm-1.0 lua libcurl) -lm
LDLIBS += $(LDLIBS_$(PROFILE))

LDFLAGS_sanitize = -fsanitize=address
LDFLAGS += $(LDFLAGS_$(PROFILE))

PREFIX ?= /usr

all: debug
debug sanitized release:
	$(MAKE) PROFILE=$@ ukko

ukko: $(OBJS)

install: ukko
	cp ukko $(PREFIX)/bin
	cp ukko.service /etc/systemd/system

format:
	clang-format -i $(SRCS) $(HDRS)

clean:
	rm -f $(OBJS) ukko *.d

-include *.d
