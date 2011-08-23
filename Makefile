IDIR = include
ODIR = build
CDIR = src
BDIR = bin
CC = gcc
CFLAGS = -I$(IDIR)
LIBS = -levent -lcurl -lpthread

CDIRS = $(CDIR)

vpath %.h $(CDIR)
vpath %.c $(CDIR)

_OBJ = main.o daq_utils.o net_utils.o pouch.o json.o xl3_utils.o xl3_rw.o crate_init.o mtc_utils.o db.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS = $(_OBJ:.o=.h) xl3_types.h 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(IDIR)/%: %
	cp $^ $(IDIR)/.

all: penn_daq tut $(DEPS)

penn_daq: $(OBJ)
	$(CC) -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS) 

tut:
	python ./tut_gen.py
	$(CC) -lreadline -lncurses -o $(BDIR)/tut $(CDIR)/tut.c $(CFLAGS)
    
clean: 
	rm -f $(ODIR)/*.o $(CDIR)/*~ core $(IDIR)/* $(BDIR)/*
