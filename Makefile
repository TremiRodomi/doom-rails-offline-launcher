CC = i686-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra -static-libgcc
LDLIBS = -ladvapi32 -luser32 -lshell32

.PHONY: all test-build package clean
all: build/Play\ Doom\ Rails.exe

build:
	mkdir -p build

build/Play\ Doom\ Rails.exe: src/offline_launcher.c | build
	$(CC) $(CFLAGS) -municode -mwindows -o "$@" $< $(LDLIBS)

test-build: build/test_multiplayer_patch.exe build/test_steam_launcher.exe

build/test_%.exe: tests/test_%.c src/offline_launcher.c | build
	$(CC) $(CFLAGS) -o "$@" $< $(LDLIBS)

package: all
	python3 package.py

clean:
	rm -rf build dist
