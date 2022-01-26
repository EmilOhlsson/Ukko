.PHONY: all debug sanitized release format clean

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
CXXFLAGS += $$(pkg-config --cflags cairomm-1.0)
CXXFLAGS += $(CXXFLAGS_$(PROFILE))

LDLIBS_sanitize += -static-libasan
LDLIBS += -lstdc++ -lfmt -lgpiod $$(pkg-config --libs cairomm-1.0) -lcurl -lm
LDLIBS += $(LDLIBS_$(PROFILE))

LDFLAGS_sanitize = -fsanitize=address
LDFLAGS += $(LDFLAGS_$(PROFILE))

all: debug
debug sanitized release:
	$(MAKE) PROFILE=$@ ukko

ukko: $(OBJS)

format:
	clang-format -i $(SRCS) $(HDRS)

clean:
	rm -f $(OBJS) ukko *.d

-include *.d
