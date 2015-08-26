# To build this on FreeBSD install lua5.2 and sqlite3
#
# on Linux you must install:
# 	liblua5.2-dev or similar
#	libsqlite3-dev or similar 

LUA_VERSION=5.2

# next 3 lines where tested on FreeBSD and Ubuntu
LUA_CFLAGS=`pkg-config --cflags --silence-errors lua$(LUA_VERSION) || pkg-config --cflags --silence-errors lua-$(LUA_VERSION)`
LUA_LIB=`pkg-config --libs --silence-errors lua$(LUA_VERSION) || pkg-config --libs --silence-errors lua-$(LUA_VERSION)`
SQLITE_FLAGS=`pkg-config --cflags --silence-errors sqlite3`

all: lua.so

lua.so: lua.o
	ld -shared -o lua.so lua.o $(LUA_LIB)

lua.o: src/lua.c
	cc -c $(LUA_CFLAGS) $(SQLITE_FLAGS) -O3 src/lua.c
	
clean:
	rm lua.o
