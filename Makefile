default: lightbridge-js lightbridge-lua

lightbridge-js: main_duktape.c common.h
	gcc -fPIC main_duktape.c \
		-lduktape -lbcm2835 -lm -o lightbridge-js

lightbridge-lua: main_lua.c common.h
	gcc -fPIC main_lua.c \
		-llua -lbcm2835 -lm -o lightbridge-lua
