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
SRCFILES += mas.c mmu.c
SRCFILES += vec.S vec.c
SRCFILES += ldr.S ldr.c
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
	@$(MAKE) -s clean
	@$(MAKE) -s includes
	@$(MAKE) -s build

build:
	@mkdir -p $(OBJDIR)
	@$(MAKE) -s $(TARGET)

includes:
	@echo "including $(TARGET)"
	@mkdir -p $(INCLUDESDIR)/$(TARGET)
	@cp -R $(INCDIR)/* $(INCLUDESDIR)/$(TARGET)/

clean:
	@echo "cleaning $(TARGET)"
	@rm -rf $(INCLUDESDIR)/$(TARGET)
	@rm -rf $(BINDIR)/$(TARGET).elf
	@rm -rf $(BINDIR)/$(TARGET).bin
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
	@echo "building $(TARGET)"
	@$(LD) $(LFLAGS) -o $(BINDIR)/$(TARGET).elf $(OBJS) $(OBJECTS)
	@$(OBJCOPY) $(OBJCOPYFLAGS) $(BINDIR)/$(TARGET).elf $(BINDIR)/$(TARGET).bin

objdump:
	$(OBJDUMP) -D $(BINDIR)/$(TARGET).elf

xxd:
	xxd $(BINDIR)/$(TARGET).bin
