.PHONY: all format clean

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.hpp)
OBJS = $(SRCS:.cpp=.o)

CXXFLAGS += -Wall -Wextra 
CXXFLAGS += -MMD -Og
CXXFLAGS += -std=c++20

LDLIBS = -lstdc++ -lfmt

all: ukko
format:
	clang-format -i $(SRCS) $(HDRS)
clean:
	rm -f $(OBJS) ukko

-include: %.d
