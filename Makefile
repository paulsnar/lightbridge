default: lb2

lb2: main2.c
	gcc -Wall -Wpedantic -fPIC -olb2 main2.c -lm -lbcm2835 -llua

# vim: set ts=4 sts=4 noet sw=4: #
