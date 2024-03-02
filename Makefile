CC=g++

all: main.o chip clean

chip: main.o
	$(CC) -o $@ $< -lsfml-system -lsfml-window -lsfml-graphics -lSDL2 -lpthread
	
main.o: main.cc
	$(CC) -c -g -o $@ $< -Wall -I/usr/include/SDL2
	
clean:
	rm -f main.o