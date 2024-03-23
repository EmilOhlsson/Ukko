.PHONY: all Debug Release RelWithDebInfo install format clean

PREFIX ?= /usr

all: Debug
Debug Release RelWithDebInfo:
	cmake -B build -DCMAKE_BUILD_TYPE=$@ -DCMAKE_UNITY_BUILD=True
	cmake --build build

ukko: $(OBJS)

install: ukko
	cp ukko $(PREFIX)/bin
	cp ukko.service /etc/systemd/system

format:
	clang-format -i src/*.cpp src/*.hpp
