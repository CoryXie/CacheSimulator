all:  sim run

sim: 
	make -C code

run:
	sh runPublic
	
clean:
	make clean -C code