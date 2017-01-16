CFLAGS = -march=native -std=c11 -I../../include 

build:
	gcc $(CFLAGS) -shared -o db.so -Wl,-rpath=libraries/libpq -L../libpq -lone db.c