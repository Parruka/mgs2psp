# MGS2 GE replay (main.c). Build: PATH=<pspdev>/bin:$PATH make -f /mnt/project-files/mgs2/psp/ge_replay/Makefile
# from an empty build directory (XCFLAGS=-DGELIST XOBJS=gelist.o: mode 1 drawn by ge/gelist.c). Copy EBOOT.PBP and deck_ge.bin (tools/gsref.c GSREF_GEOUT) to ms0:/PSP/GAME/MGS2GE/.
TARGET = MGS2GE
SRC := /mnt/project-files/mgs2/psp/ge_replay
VPATH = $(SRC):$(SRC)/../../ge
OBJS = main.o $(XOBJS)
CFLAGS = -O2 -G0 -Wall -D_PSP_FW_VERSION=600 $(XCFLAGS)
LIBS = -lpspgu -lpsppower -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = MGS2 GE replay v6
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
