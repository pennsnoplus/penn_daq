IDIR = include
ODIR = build
CDIR = src
BDIR = bin
CC = gcc
CFLAGS = -I$(IDIR)
LIBS = -levent -lcurl

_OBJ = main.o daq_utils.o net_utils.o pouch.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS = $(_OBJ:.o=.h) xl3_types.h 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))


$(ODIR)/%.o: $(CDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

penn_daq: $(OBJ)
	$(CC) -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS) 
    
clean: 
	rm -f $(ODIR)/*.o $(CDIR)/*~ core $(IDIR)/*~ $(BDIR)/*
