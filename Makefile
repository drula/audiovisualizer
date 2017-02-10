СС = gcc
FLAGS = `pkg-config --cflags --libs gstreamer-1.0 gstreamer-plugins-base-1.0`
ADDRESS = /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstaudiovisualizer.so
OBJ = gstaudiovisualizer.o

install: $(OBJ)
	$(CC) -shared -o $(ADDRESS) $(OBJ)
	make clean

$(OBJ): gstaudiovisualizer.c gstaudiovisualizer.h
	$(CC) -fPIC gstaudiovisualizer.c -c $(FLAGS)

clean:
	rm $(OBJ)

unistall:
	rm $(ADDRESS)
