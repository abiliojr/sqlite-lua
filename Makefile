# To build this on FreeBSD install lua5.3 (or similar) and sqlite3
#
# on Linux you must install:
# 	liblua5.3-dev or similar
#	libsqlite3-dev or similar 


LUA_VERSION=5.3

# next 3 lines where tested on FreeBSD and Ubuntu
LUA_CFLAGS=`pkg-config --cflags --silence-errors lua$(LUA_VERSION) || pkg-config --cflags --silence-errors lua-$(LUA_VERSION) || pkg-config --cflags --silence-errors lua`
LUA_LIB=`pkg-config --libs --silence-errors lua$(LUA_VERSION) || pkg-config --libs --silence-errors lua-$(LUA_VERSION) || pkg-config --libs --silence-errors lua`
SQLITE_FLAGS=`pkg-config --cflags --silence-errors sqlite3`

all: lua.so

lua.so: lua.o
	cc -shared -o lua.so lua.o $(LUA_LIB)
	@[ "`uname -s`" == "Darwin" ] && mv lua.so lua.dylib || :

lua.o: src/lua.c
	cc -c $(LUA_CFLAGS) $(SQLITE_FLAGS) -O3 src/lua.c
	
clean:
	rm lua.o
