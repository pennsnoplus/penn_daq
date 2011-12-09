IDIR = include
ODIR = build
CDIR = src
BDIR = bin
CC = gcc
#CFLAGS = -I$(IDIR)
CFLAGS = $(patsubst %,-I%,$(CDIRS))
LIBS = -lcurl -lpthread -lm

_CDIRS = core utils db net crate xl3 fec mtc tut tests
CDIRS = $(patsubst %,$(CDIR)/%,$(_CDIRS))

vpath %.h $(CDIRS)
vpath %.c $(CDIRS)

_OBJ = main.o daq_utils.o net_utils.o pouch.o json.o xl3_utils.o xl3_rw.o crate_init.o mtc_utils.o mtc_cmds.o db.o net.o process_packet.o mtc_init.o mtc_rw.o fec_test.o xl3_cmds.o fec_cmds.o run_pedestals.o fec_utils.o trigger_scan.o mem_test.o vmon.o board_id.o ped_run.o zdisc.o crate_cbal.o cgt_test.o cmos_m_gtvalid.o cald_test.o ttot.o fifo_test.o mb_stability_test.o chinj_scan.o final_test.o disc_check.o see_refl.o find_noise.o ecal.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS = $(_OBJ:.o=.h) packet_types.h db_types.h mtc_registers.h xl3_registers.h unpack_bundles.h dac_number.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

#$(ODIR)/%.o: %.c $(DEPS)
$(ODIR)/%.o: %.c $(_DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

#$(IDIR)/%: %
#	cp $^ $(IDIR)/.

#all: penn_daq tut $(DEPS)
all: directories penn_daq tut

directories:
	test -d $(ODIR) || mkdir $(ODIR) 
	test -d $(BDIR) || mkdir $(BDIR) 
	test -d logs || mkdir logs 
	test -d macro || mkdir macro 

penn_daq: $(OBJ)
	$(CC) -g -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS) 

tut:
	python $(CDIR)/tut/tut_gen.py
	$(CC) -lreadline -lncurses -o $(BDIR)/tut $(CDIR)/tut/tut.c $(CFLAGS)
    
clean: 
	rm -f $(ODIR)/* $(BDIR)/*
#rm -f $(ODIR)/*.o core $(IDIR)/* $(BDIR)/*
