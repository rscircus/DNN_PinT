CC     =gcc
CXX    =g++
MPICC  = mpicc
MPICXX = mpicxx

INC = -I. -I$(BRAID_INC_DIR) -I$(CODI_DIR)
BRAID_INC_DIR = /home/sguenther/Software/braid_llnl/
BRAID_LIB_FILE = /home/sguenther/Software/braid_llnl/libbraid.a
CODI_DIR = /home/sguenther/Software/CoDiPack_v1.0/include/

# set compiler flags
CFLAGS= -g -Wall -pedantic -lm -Wno-write-strings

DEPS = lib.h
OBJ-serial = main-serial.o lib.o
OBJ-pint   = main.o lib.o bfgs.o

%.o: %.c $(DEPS)
	$(MPICXX) $(CFLAGS) -c $< -o $@  $(INC)

main: $(OBJ-pint)
	$(MPICXX) $(CFLAGS) -o $@ $^ $(BRAID_LIB_FILE)

main-serial: $(OBJ-serial)
	$(MPICXX) $(CFLAGS) -o $@ $^ 

clean: 
	rm -f *.o
	rm -f main-serial
	rm -f main
