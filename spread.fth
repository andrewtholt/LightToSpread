
load dynamic.fth

-1 value libspread
-1 value buffer
-1 value sender

255 allocate abort" alloctae failed" to buffer
255 allocate abort" allocate failed" to sender

s" ./libConnectToSpread.so" dlopen abort" Failed" to libspread

0 0 s" dumpSymbols" libspread dlsym abort" Not found" mkfunc dump-symbols
0 0 s" defaultSymbols" libspread dlsym abort" Not found" mkfunc default-symbols
0 0 s" dumpGlobals" libspread dlsym abort" Not found" mkfunc dump-globals

0 1 s" loadFile"    libspread dlsym abort" Not found" mkfunc load-file
0 0 s" saveSymbols" libspread dlsym abort" Not found" mkfunc save-symbols


0 0 s" connectToSpread" libspread dlsym abort" Not found" mkfunc connect2spread
1 1 s" getSymbol" libspread dlsym abort" Not found" mkfunc get-symbol

0 2 s" setSymbolValue" libspread dlsym abort" Not found" mkfunc set-symbol-value
0 2 s" setBoolean" libspread dlsym abort" Not found" mkfunc set-boolean
0 2 s" toSpread" libspread dlsym abort" Not found" mkfunc to-spread
1 2 s" fromSpread" libspread dlsym abort" Not found" mkfunc from-spread

: init
	buffer 255 erase
	sender 255 erase

	default-symbols
	dump-symbols
;

: test
	init
	s" tst.rc" drop load-file
	dump-symbols
	connect2spread
;

