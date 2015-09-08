all: libkt.a ktool

kt.o: kt.c kt.h
	$(CC) $(CFLAGS) -c kt.c -lcrypto -lssl -lcurl
	
libkt.a: kt.o
	ar -rcs libkt.a kt.o
	
ktool: ktool.c libkt.a
	$(CC) $(CFLAGS) -o ktool ktool.c -L. -lkt -lcrypto -lssl -lcurl
	
clean:
	rm -f *.o ktool libkt.a