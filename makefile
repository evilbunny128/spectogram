all:
	gcc main.c -o main -lpulse -lpulse-simple -lm -lSDL2 -O2 -ftree-vectorize
