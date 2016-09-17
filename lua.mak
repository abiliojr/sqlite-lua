# to run this: "nmake -f lua.mak"
# please extract the lua src directory inside a folder called lua
# extract sqlite3.h and sqlite3ext.h inside a folder called sqlite

# it should generate lua.dll as the output

CPP=cl.exe


ALL :
	@if not exist ".\lua\lua.h" echo "please extract the lua src directory contents inside the lua folder"
	@if not exist ".\lua" mkdir lua

	@if not exist ".\sqlite\sqlite3.h" echo "please extract SQLite's header files inside the sqlite folder"
	@if not exist ".\sqlite" mkdir sqlite

	# we're going to use wildcards, and lua\lua.c is not needed (actually, it conflicts as it has it's own main).
	@if exist ".\lua\lua.c" rm .\lua\lua.c

	$(CPP) /Ot /GD /nologo /W3 /I .\lua /I .\sqlite .\lua\*.c .\src\lua.c -link -dll -out:lua.dll
	@erase *.obj
	@erase lua.exp lua.lib
