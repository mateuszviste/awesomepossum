CFLAGS = -O0 -g -std=gnu89 -Wall -Wextra -pedantic
CLIBS = -lrt -lSDL -lSDL_image

all: game edit

sprites.h: *.png
	rm -f sprites.h
	for f in *.png ; do optipng -o7 $$f ; xxd -i $$f >> sprites.h ; done

levels.h: lev*.dat
	rm -f levels.h
	for f in lev*.dat ; do xxd -i $$f >> levels.h ; done

game: platform.c sprites.h levels.h
	gcc $(CLIBS) platform.c $(CFLAGS) -o game

edit: edit.c sprites.h
	gcc $(CLIBS) edit.c $(CFLAGS) -o edit

clean:
	rm -f game edit *.o
