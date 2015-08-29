/*
*
* SQLite's Moon: Lua for SQLite
*
* Create new SQL functions! Write them using Lua.
*
* By: Abilio Marques <https://github.com/abiliojr/>
*
*
* After loading this plugin, you can use the newly function "createlua" to create your own
* functions.
*
* See README.md for more information on how to use it.
*
* This code is published under the Simplified BSD License
*
*/

#include <math.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1;
#if defined( _WIN32 )
#define _USE_MATH_DEFINES
#endif /* _WIN32 */


//
// Get a new LUA state
//

static void *createLuaState(void) {
    lua_State *L;

    L = luaL_newstate();
    if (L == NULL) return NULL;

    luaL_openlibs(L);
    return L;
}


//
// Destroy a previously allocated LUA state
//

static void destroyLuaState(void *state) {
	if (state != NULL) {
		lua_gc(state, LUA_GCCOLLECT, 0);
		lua_close(state);
	}
}


//
// Loads an SQLite value into the LUA stack.
// Tries to match the types
//

static void pushSqliteLua(int i, sqlite3_value *value, lua_State *destination) {
	int n;
	double d;
	const char *str;

	lua_pushnumber(destination, i);

	switch (sqlite3_value_type(value)) {

		case SQLITE_FLOAT:
			d = sqlite3_value_double(value);
			lua_pushnumber(destination, d);
			break;

		case SQLITE_INTEGER:
			n = sqlite3_value_int(value);
			lua_pushinteger(destination, n);
			break;

		case SQLITE_NULL:
			lua_pushnil(destination);
			break;

		case SQLITE_TEXT:
			str = (char *) sqlite3_value_text(value);
			lua_pushstring(destination, str);
			break;

		case SQLITE_BLOB:
			str = sqlite3_value_blob(value);
			n = sqlite3_value_bytes(value);
			lua_pushlstring(destination, str, n);
			break;
	}

	lua_settable(destination, -3);

}


//
// Gets a Lua value from the stack and returns it as an SQLite value.
// Tries to match the types
//

static void popLuaSqlite(lua_State *from, sqlite3_context *ctx) {
	int n, t;
	double d;
	const char *str;

	if (lua_gettop(from) == 0) {
		sqlite3_result_null(ctx);
		return;
	}

	switch (lua_type(from, -1)) {

		case LUA_TNUMBER:
			d = lua_tonumber(from, -1);
			if (floor(d) == d) {
				sqlite3_result_int(ctx, (int) d);
			} else {
				sqlite3_result_double(ctx, d);
			}
			break;

		case LUA_TSTRING:
			str = lua_tostring(from, -1);
			sqlite3_result_text(ctx, str, -1, SQLITE_TRANSIENT);
			break;

		case LUA_TBOOLEAN:
			n = lua_toboolean(from, -1);
			sqlite3_result_int(ctx, n);
			break;

		case LUA_TNIL:
			sqlite3_result_null(ctx);
			break;

		default:
			sqlite3_result_error(ctx, "Unsupported return type", -1);
			break;
	}

	lua_pop(from, 1);
}


static lua_State *getAssociatedLuaState(sqlite3_context *ctx) {
	return sqlite3_user_data(ctx);
}


//
// Loads a Lua chunk into the stack
//

static void pushLuaChunk(lua_State *where, const unsigned char *code, const unsigned char *init,
	const unsigned char *final) {
	
	lua_pop(where, lua_gettop(where)); // clean the stack

	luaL_loadstring(where, (char *) code);

	if (init != NULL && final != NULL) {
		luaL_loadstring(where, (char *) init);
		luaL_loadstring(where, (char *) final);
	}
}


//
// Loads the passed values inside arg[] and executes the related Lua chunk
//

static void pushLuaParams(lua_State *where, int num_values, sqlite3_value **values) {
	int i;

	lua_createtable(where, num_values, 0);

	for (i = 0; i < num_values; i++) {
		pushSqliteLua(i + 1, values[i], where);
	}

	lua_setglobal(where, "arg");
}


//
// Executes Lua chunk previously stored in the stack
//

static void executeLuaChunk(lua_State *where, int returnsValue, int chunk) {
	lua_pushvalue(where, chunk);
	lua_pcall(where, 0, returnsValue, 0);
}



//
// Check for return value
//
static int checkStackLen(lua_State *stack, sqlite3_context *ctx, int validLen) {
	if (lua_gettop(stack) != validLen) {
		sqlite3_result_error(ctx, "Invalid Lua stack length! " \
			"This normally happens if your code doesn't return any value.", -1);
		return 0;
	}

	return 1;
}

//
// Store a pointer into a Lua table located at the bottom of a stack
//

static void storePointer(lua_State *where, int pos, const char *name, void *p) {
	lua_pushstring(where, name);
	lua_pushlightuserdata(where, p);
	lua_settable(where, pos);
}


//
// Find a pointer inside a Lua table located at the bottom of a stack
//

static lua_State *findPointer(lua_State *where, int pos, const char *name) {
	lua_State *result;
	

	lua_pushstring(where, name);
	lua_pushnil(where);  /* first key */
	while (lua_next(where, pos) != 0) {

		if (lua_compare(where, -3, -2, LUA_OPEQ)) {
			// found the item
			result = lua_touserdata(where, -1);

			// remove the remains from the stack (two copies of the name + the value)
			lua_pop(where, 3);
			return result;	
		}

		lua_pop(where, 1);
    }
    lua_pop(where, 1); // remove the name we were looking for from the stack
    return NULL;
}


//
// Executes scalar Lua function (called by SQLite)
//

static void sql_scalar_lua(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	lua_State *L;

	L = getAssociatedLuaState(ctx);
	pushLuaParams(L, num_values, values);
	executeLuaChunk(L, 1, 1);
	if (checkStackLen(L, ctx, 2)) popLuaSqlite(L, ctx); // stack size = 2 (code and return val)
}


//
// Executes init and step parts of a Lua aggregate function (called by SQLite)
//

static void sql_aggregate_lua(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	int i;
	lua_State *L;
	int *notFirstTime = sqlite3_aggregate_context(ctx, sizeof(int));

	if (notFirstTime == NULL) {
		sqlite3_result_error_nomem(ctx);
		return;
	}

	L = getAssociatedLuaState(ctx);

	if (*notFirstTime == 0) {
		// it must execute the init chunk once
		executeLuaChunk(L, 0, 2);
		*notFirstTime = 1;
	}

	pushLuaParams(L, num_values, values);
	executeLuaChunk(L, 0, 1);
}


//
// Executes the final chunk of an aggregate function (called by SQLite)
//

static void sql_aggregate_lua_final(sqlite3_context *ctx) {
	lua_State *L;
 	L = getAssociatedLuaState(ctx);

	executeLuaChunk(L, 1, 3);

	if (checkStackLen(L, ctx, 4)) popLuaSqlite(L, ctx); // stack size = 4 (3x code and return val)
}


//
// Check that all the Lua function creation parameters are strings
//

static const char *checkCreateluaParameters(int num_values, sqlite3_value **values) {
	int i, adj;

	const char *errors[] = {
		"Invalid function name, string expected",
		"Invalid function code, string expected",
		"Invalid init code, string expected",
		"Invalid step code, string expected",
		"Invalid final code, string expected"
	};

	for (i = 0; i < num_values; i++) {
		adj = (num_values == 2 || i == 0 ? 0 : 1);
		if (sqlite3_value_type(values[i]) != SQLITE_TEXT) return errors[i + adj];
	}

	return NULL;

}


//
// Create a new SQL Lua function (called by SQLite)
//

static void sql_createlua(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	lua_State *L, *functionTable;

	const char *msg, *name;

	msg = checkCreateluaParameters(num_values, values);
	if (msg != NULL) {
		sqlite3_result_error(ctx, msg, -1);
		return;
	}

	name = (char *) sqlite3_value_text(values[0]);

	functionTable = getAssociatedLuaState(ctx);

	L = findPointer(functionTable, 1, name);
	if (L == NULL) {
		L = createLuaState();
		storePointer(functionTable, 1, name, L);

		// now register the function within SQLite
		if (num_values == 2) {
			// scalar
			sqlite3_create_function_v2(sqlite3_context_db_handle(ctx), (char *) name, -1,
				SQLITE_UTF8, (void *) L, sql_scalar_lua, NULL, NULL, destroyLuaState);
		} else {
			// aggregate
			sqlite3_create_function_v2(sqlite3_context_db_handle(ctx), (char *) name, -1,
				SQLITE_UTF8, (void *) L, NULL, sql_aggregate_lua, sql_aggregate_lua_final, destroyLuaState);
		}

	}
	
	// load the code inside the stack
	if (num_values == 2) {
		// scalar
		pushLuaChunk(L, sqlite3_value_text(values[1]), NULL, NULL);
	} else {
		// aggregate
		pushLuaChunk(L, sqlite3_value_text(values[2]), sqlite3_value_text(values[1]),
			sqlite3_value_text(values[3]));
	}

	sqlite3_result_text(ctx, "ok", -1, SQLITE_TRANSIENT);
}


//
// plugin main
//

int sqlite3_extension_init(sqlite3 *db, char **error, const sqlite3_api_routines *api) {
	SQLITE_EXTENSION_INIT2(api);

	lua_State *mainState; // going to hold the list of created functions

	mainState = createLuaState();
	if (mainState == NULL) return 0;
	lua_newtable(mainState);

	sqlite3_create_function_v2(db, "createlua", 2, SQLITE_UTF8, mainState, sql_createlua,
		NULL, NULL, NULL);
	sqlite3_create_function_v2(db, "createlua", 4, SQLITE_UTF8, mainState, sql_createlua,
		NULL, NULL, destroyLuaState);

	return SQLITE_OK;
}
