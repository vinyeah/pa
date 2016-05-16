
OS=$(shell uname)

ifdef BHU_CLOUD
CFLAGS += -DBHU_CLOUD
endif

CFLAGS+=-g -I../include -Werror
#LDFLAGS+=-shared
ifeq ($(OS), Linux)
LDLIBS+=-lrt -lm -lpthread
else
LDLIBS+=-lws2_32
endif

.PHONY: all clean install

BASE_OBJS=cJSON.o comm.o config.o crc.o eloop.o inifile.o rbtree.o ctrl.o ftp.o dev.o place.o pacomm.o output_worker.o devmap.o orion_input_worker.o input_worker.o sta_sniffer_worker.o

TARGETS?=pa_server

OBJS=$(BASE_OBJS) main.o

TARGETS2?=pa_server_ctrl
OBJS2=$(BASE_OBJS) appctrl.o

all:$(TARGETS) $(TARGETS2)

$(TARGETS):$(OBJS)
	    $(CC) $(OUTPUT_OPTION) $^ $(LDFLAGS) $(LDLIBS)

$(TARGETS2):$(OBJS2)
	    $(CC) $(OUTPUT_OPTION) $^ $(LDFLAGS) $(LDLIBS)

clean:
	    @rm -f $(OBJS) $(TARGETS)
	    @rm -f $(OBJS2) $(TARGETS2)
