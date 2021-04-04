CC := sdcc -mmcs51
CPP := cpp
SHELL := /bin/bash
AWK := awk

CFLAGS := -I.
SRC := slave_sync_8.c
DEP := $(patsubst %.c,%.d,$(SRC))
TARGET := $(patsubst %.c,%.inc,$(SRC))
.PRECIOUS: $(patsubst %.c,%.ihx,$(SRC))

all: $(DEP)
	@$(MAKE) $(TARGET)

clean:
	$(RM) $(DEP) $(TARGET) $(basename $(SRC)).{asm,lk,lst,map,mem,rel,rst,sym,ihx,h}

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(DEP)
endif

%.d: %.c
	$(info GEN $@)
	@$(CPP) -MM $(CFLAGS) $< | sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@

%: %.d

%.ihx: %.c
	$(CC) $(CFLAGS) $(SRC)

%.inc: %.ihx
	@$(AWK) '{print "\""$$0"\","}' $< > $@

