IDIR = include
ODIR = build
CDIR = src
BDIR = bin
CC = gcc
CFLAGS = -I$(IDIR)
LIBS = -levent -lcurl -lpthread

_OBJ = main.o daq_utils.o net_utils.o pouch.o xl3_utils.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS = $(_OBJ:.o=.h) xl3_types.h 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))


$(ODIR)/%.o: $(CDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: penn_daq tut

penn_daq: $(OBJ)
	$(CC) -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS) 

tut:
	python ./tut_gen.py
	$(CC) -lreadline -lncurses -o $(BDIR)/tut $(CDIR)/tut.c $(CFLAGS)
    
clean: 
	rm -f $(ODIR)/*.o $(CDIR)/*~ core $(IDIR)/*~ $(BDIR)/*
