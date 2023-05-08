#
# Makefile
#
# For 'mercury'
#

PRJ := mercury


CC      ?= gcc
CPP     ?= cpp
CXX     ?= g++
LD      ?= ld
INSTALL ?= install
RM      ?= rm
ECHO    ?= echo


CSTD    := gnu11
CXXSTD  := gnu++14
OPTLVL  := 3
DBGLVL  := 0


DEFINES := -DNDEBUG
DEFINES += -UDEBUG


INC_DIRS := -I./include

LIBS     := -lasound
LIB_DIRS := 


LDFLAGS  := -lasound
CFLAGS   := -std=$(CSTD) -O$(OPTLVL) -g$(DBGLVL) $(DEFINES) -Wall -Wno-format $(INC_DIRS)
CXXFLAGS := -std=$(CXXSTD) -O$(OPTLVL) -g$(DBGLVL) $(DEFINES) -Wall -Wno-format $(INC_DIRS)
LDFLAGS  := $(LIB_DIRS) $(LIBS)


CXX_SRCS := $(wildcard source/*.cc)


$(PRJ): $(CXX_SRCS)
	$(CXX) $(CXX_SRCS) $(LDFLAGS) $(CXXFLAGS) -o $@


.PHONY: clean install


install:
	$(INSTALL) -D $(PRJ) /usr/bin/mercury


clean:
	$(RM) -f $(PRJ)
	$(RM) -f *.o
	$(RM) -f *.map *.lst
