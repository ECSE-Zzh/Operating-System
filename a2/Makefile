CC=gcc
CFLAGS=-DFRAME_STORE_SIZE=$(X) -DVARIABLE_STORE_SIZE=$(Y)

myshell: shell.c interpreter.c shellmemory.c
	$(CC) $(CFLAGS) -c -g shell.c interpreter.c shellmemory.c kernel.c pcb.c ready_queue.c
	$(CC) $(CFLAGS) -o myshell shell.o interpreter.o shellmemory.o kernel.o pcb.o ready_queue.o 
clean: 
	rm myshell; rm *.o
