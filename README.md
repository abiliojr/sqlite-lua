# SQLite's Moon: Lua for SQLite

Create new SQL functions! Write them using [Lua](http://www.lua.org/).


# Usage

This is an [SQLite](http://sqlite.org/) plugin. After loading it, you can use the function ```createlua``` to define your own functions, like this:

```
SELECT createlua({parameters});
```

This query will return ```ok``` if everything went fine.

SQL supports two types of functions:
  * Scalar: returns a result for each record.
  * Aggregate: process all records, then return a single final result.


## Scalar

When creating a scalar function, you must provide 2 parameters, in the following order:
  * Name: the function you are defining.
  * Code: well... the Lua code. Returns the result.

For example, to create a function that calculates the cosine of an angle:
```
SELECT createlua('cos', 'return math.cos(arg[1])');
```

And now you can calculate cosines on a query:
```
CREATE TABLE t(angle NUMERIC);
INSERT INTO t(angle) VALUES (0),(1.571),(3.1416);

SELECT cos(angle) FROM t; -- should return approximately {1, 0, -1}
```


## Aggregate

When creating aggregate functions, you must provide 4 parameters, in the following order:
  * Name:  the function you are defining.
  * Init:  this code will execute before the first record is processed.
  * Step:  code called on each record.
  * Final: code for the last step after all records have been processed. Returns the result.

For example, to create a function that calculates the [geometric mean](https://en.wikipedia.org/wiki/Geometric_mean) of a set of numbers:

```
SELECT createlua('gmean',
                 'prod = 1; n = 0;',
                 'n = n + 1; prod = prod * arg[1];',
                 'return prod ^ (1/n);');
```

And now, used on a query:

```
CREATE TABLE data(val NUMERIC);
INSERT INTO data(val) VALUES (2), (4), (8);

SELECT gmean(val) FROM data; -- should return 4
```


# Parameters

Parameters supplied to the functions can be accessed from within Lua. The array: ```arg[i]``` contains the values. ```arg``` is a zero-based array (1 <= i <= n), like is customary in Lua.

Example:
```
-- this is a pattern matching example using Lua's internal function
SELECT createlua('regex', 'return string.match(arg[1], arg[2])');

SELECT regex('abc 24 ef', '([0-9]+)'); -- Should return 24
```


# Reading from files

You can use the auxiliary function named ```loadfile``` to get the content of a file. This is useful to declare long functions. The signature is:

```
loadfile(filename,[type])
```

For example, to create a function which source code is stored in a file named ```longcode.lua```, do:

```
select createlua('longcode', select loadfile('longcode.lua'));
```

```loadfile``` can take an optional type parameter. If type is 'b', the file content will be read as a blob. This can be useful for other applications, e.g., reading binary files inside the database.


# Notes

* Functions must always return a value. For aggregates, this is only performed on the final step.
* You can redefine a function at any time by calling ```createlua``` again. This holds true even for SQLite's native functions.
* Currently, you can't return an array into SQLite.
* Even when Lua can return multiple values simultaneously, only the first one will be passed back to SQLite.


# Building

On FreeBSD, you must have ```lua5.3```and ```sqlite3``` installed. For Linux (Ubuntu flavored), the equivalents are ```liblua5.3-dev``` and ```libsqlite3-dev```. The library names and their locations could be different on other Operating Systems. If that's the case, you may need to edit the Makefile.

On Windows, using Visual Studio, you can use the provided ```lua.mak```. You'll need to extract the Lua src directory content in a folder named lua. Also the files sqlite3.h and sqlite3ext.h, must be extracted inside a folder named sqlite. These files are part of the [sqlite source code amalgamation](https://www.sqlite.org/download.html). Afterwards, you can compile the dll like this:
```
nmake -f lua.mak
```

This code should remain compatible with future versions of Lua. It has also been tested with Lua 5.2. You'll only need to alter the ```LUA_VERSION``` parameter inside the Makefile.


# Loading

To use this plugin from within the sqlite3 command line tool, you must run:

```.load path-to-plugin/lua``` (for security reasons, SQLite needs you to provide the library path if the plugin is not located inside ```LD_LIBRARY_PATH```. Current directory '.' is a valid path, e.g., ```.load ./lua```).

You could also use this from within almost any code that uses sqlite3 as a library. Refer to SQLite website to get more information about ```sqlite3_load_extension``` and ```sqlite3_enable_load_extension``` or look for their equivalents in your language bindings' documentation.


## License

The work here contained is published under the Simplified BSD License. Read LICENSE for more information.
