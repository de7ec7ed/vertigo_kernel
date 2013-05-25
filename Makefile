TARGET := kernel

MKDIR := ../make
BINDIR := ../binaries
INCLUDESDIR := ../includes
OBJECTSDIR := ../objects

include $(MKDIR)/Makefile.common

HDRFILES := $(INCLUDESDIR)/defines.h $(INCLUDESDIR)/types.h
HDRFILES += $(INCDIR)/start.h
HDRFILES += $(INCDIR)/mas.h
HDRFILES += $(INCDIR)/mmu.h
HDRFILES += $(INCDIR)/vec.h
HDRFILES += $(INCDIR)/ldr.h

SRCFILES := start.S start.c
SRCFILES += call.c
SRCFILES += mas.c
SRCFILES += mmu.c
SRCFILES += vec.S vec.c
SRCFILES += log.c
SRCFILES += ldr.S ldr.c
SRCFILES += lst.c
SRCFILES += end.S

include $(MKDIR)/Makefile_arm_fb.common

OBJECTS := $(OBJECTSDIR)/stdlib.o 
OBJECTS += $(OBJECTSDIR)/armv7lib.o 
OBJECTS += $(OBJECTSDIR)/dbglib.o
OBJECTS += $(OBJECTSDIR)/fxplib.o

.PHONY : all
.PHONY : build
.PHONY : includes
.PHONY : clean

all:
	@$(MAKE) -s check
	@$(MAKE) -s clean
	@$(MAKE) version
	@$(MAKE) -s includes
	@$(MAKE) -s build

build:
	@$(MAKE) -s check
	@mkdir -p $(OBJDIR)
	@$(MAKE) -s $(TARGET)

version:
	@echo "#ifndef __$(shell echo $(TARGET) | tr a-z A-Z)_VERSION_H__" > $(INCDIR)/version.h
	@echo "#define __$(shell echo $(TARGET) | tr a-z A-Z)_VERSION_H__" >> $(INCDIR)/version.h
	@echo "#define VERSION_RELEASE_NUMBER \"0.1\"" >> $(INCDIR)/version.h
	@echo "#define VERSION_COMMIT_NUMBER \"`git log -1 --pretty=format:%H`\"" >> $(INCDIR)/version.h
	@echo "#define VERSION_BUILD_DATE_TIME \"`date`\"" >> $(INCDIR)/version.h
	@echo "#endif //__$(shell echo $(TARGET) | tr a-z A-Z)_VERSION_H__" >> $(INCDIR)/version.h

includes:
	@echo "including $(TARGET)"
	@mkdir -p $(INCLUDESDIR)/$(TARGET)
	@cp -R $(INCDIR)/* $(INCLUDESDIR)/$(TARGET)/

check:
	@echo "checking toolchain"
	@if test "$(PREFIX)" = "" ; then echo "A toolchain has not been specified. Define PREFIX, for example \"make PREFIX=arm-linux-gnueabi-\""; exit 2; fi
	@if ! [ -x "`which $(LD) 2>/dev/null`" ]; then echo LD = "$(LD) not found"; exit 2; fi
	@if ! [ -x "`which $(CC) 2>/dev/null`" ]; then echo "CC = $(CC) not found"; exit 2; fi
	@if ! [ -x "`which $(AR) 2>/dev/null`" ]; then echo "AR = $(AR) not found"; exit 2; fi
	@if ! [ -x "`which $(NM) 2>/dev/null`" ]; then echo "NM = $(NM) not found"; exit 2; fi
	@if ! [ -x "`which $(OBJDUMP) 2>/dev/null`" ]; then echo "OBJDUMP = $(OBJDUMP) not found"; exit 2; fi
	@if ! [ -x "`which $(OBJCOPY) 2>/dev/null`" ]; then echo "OBJCOPY = $(OBJCOPY) not found"; exit 2; fi

clean:
	@echo "cleaning $(TARGET)"
	@rm -rf $(INCLUDESDIR)/$(TARGET)
	@rm -rf $(BINDIR)/$(TARGET).elf
	@rm -rf $(BINDIR)/$(TARGET).bin
	@rm -f $(INCDIR)/version.h
	@rm -rf $(OBJDIR)
	@rm -rf *~
	@rm -rf $(INCDIR)/*~
	@rm -rf $(SRCDIR)/*~

$(OBJDIR)/%.S.o: $(SRCDIR)/%.S $(DEPS)
	@echo "assembling $<"
	@$(CC) $(AFLAGS) -c -D__ASSEMBLY__ -o $@ $<

$(OBJDIR)/%.c.o: $(SRCDIR)/%.c $(DEPS)
	@echo "compiling $<"
	@$(CC) $(CFLAGS) -c -D__C__ -o $@ $<

$(TARGET): $(OBJS)
	echo "building $(TARGET)"
	@$(LD) $(LFLAGS) -o $(BINDIR)/$(TARGET).elf $(OBJS) $(OBJECTS)
	@$(OBJCOPY) $(OBJCOPYFLAGS) $(BINDIR)/$(TARGET).elf $(BINDIR)/$(TARGET).bin

objdump:
	@$(OBJDUMP) -D $(BINDIR)/$(TARGET).elf

xxd:
	@xxd $(BINDIR)/$(TARGET).bin
