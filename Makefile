.PHONY: all format clean

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.hpp)
OBJS = $(SRCS:.cpp=.o)

CXXFLAGS += -Wall -Wextra 
CXXFLAGS += -MMD -Og -g
CXXFLAGS += -std=c++20

LDLIBS = -lstdc++ -lfmt

ukko: $(OBJS)

all: ukko
format:
	clang-format -i $(SRCS) $(HDRS)
clean:
	rm -f $(OBJS) ukko *.d

-include *.d
