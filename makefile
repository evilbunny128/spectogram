all:
	gcc main.c -o main -lasound -lm -lSDL2 -O2 -ftree-vectorize
