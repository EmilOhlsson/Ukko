.PHONY: debug sanitized release format clean

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.hpp)
OBJS = $(SRCS:.cpp=.o)

CC = g++
CXX = g++
DUMMY ?= 1

CXXFLAGS_SANITIZE = -fsanitize=address -fno-omit-frame-pointer
CXXFLAGS_RELEASE = -O3
CXXFLAGS_DEBUG = -g -Og

CXXFLAGS += -Wall -Wextra 
CXXFLAGS += -DDUMMY=$(DUMMY)
CXXFLAGS += -MMD
CXXFLAGS += -std=c++20
CXXFLAGS += $$(pkg-config --cflags cairomm-1.0)
CXXFLAGS += $(CXXFLAGS_$(PROFILE))

LDLIBS_SANITIZE += -static-libasan
LDLIBS += -lstdc++ -lfmt -lgpiod $$(pkg-config --libs cairomm-1.0) -lcurl
LDLIBS += $(LDLIBS_$(PROFILE))

LDFLAGS_SANITIZE = -fsanitize=address
LDFLAGS += $(LDFLAGS_$(PROFILE))

debug sanitized release:
	$(MAKE) PROFILE=$(call uc,$@) ukko

ukko: $(OBJS)

format:
	clang-format -i $(SRCS) $(HDRS)

clean:
	rm -f $(OBJS) ukko *.d

-include *.d
