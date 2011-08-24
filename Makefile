IDIR = include
ODIR = build
CDIR = src
BDIR = bin
CC = gcc
#CFLAGS = -I$(IDIR)
CFLAGS = $(patsubst %,-I%,$(CDIRS))
LIBS = -levent -lcurl -lpthread

_CDIRS = core utils db net crate xl3 fec mtc tut
CDIRS = $(patsubst %,$(CDIR)/%,$(_CDIRS))

vpath %.h $(CDIRS)
vpath %.c $(CDIRS)

_OBJ = main.o daq_utils.o net_utils.o pouch.o json.o xl3_utils.o xl3_rw.o crate_init.o mtc_utils.o db.o net.o process_packet.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS = $(_OBJ:.o=.h) packet_types.h db_types.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

#$(ODIR)/%.o: %.c $(DEPS)
$(ODIR)/%.o: %.c $(_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(IDIR)/%: %
	cp $^ $(IDIR)/.

all: penn_daq tut $(DEPS)

penn_daq: $(OBJ)
	$(CC) -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS) 

tut:
	python $(CDIR)/tut/tut_gen.py
	$(CC) -lreadline -lncurses -o $(BDIR)/tut $(CDIR)/tut/tut.c $(CFLAGS)
    
clean: 
	rm -f $(ODIR)/*.o core $(IDIR)/* $(BDIR)/*