# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS =$(shell uname -s)

MAKE = make

ifeq ($(HOSTOS),Darwin)

CC64 = x86_64-elf-gcc
OBJCOPY = x86_64-elf-objcopy

SEDNOBAK = sed -i ''

else

CC64 = gcc
OBJCOPY = objcopy

SEDNOBAK = sed -i

endif

DOCSGEN = doxygen
DOCSFILES = $(shell find . -type f -name \*.md)
DOCSCONF = docs.doxygen
INCLUDESDIR = includes

CCXXFLAGS += -std=gnu11 -O2 -nostdlib -nostdinc -ffreestanding -fno-builtin -c -I$(INCLUDESDIR) \
	-Werror -Wall -Wextra -ffunction-sections -fdata-sections \
	-mno-red-zone -fstack-protector-all -fno-omit-frame-pointer \
    -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
    -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
    -Wstrict-prototypes \
	-fpic -fPIC -fno-plt -mcmodel=large -fno-jump-tables -fno-ident -fno-asynchronous-unwind-tables ${CCXXEXTRAFLAGS}

CXXTESTFLAGS= -D___TESTMODE=1

CC64FLAGS    = -m64 -march=x86-64 -D___BITS=64 -msse4.2 $(CCXXFLAGS)
CC64INTFLAGS = -m64 -march=x86-64 -mgeneral-regs-only -D___BITS=64 $(CCXXFLAGS)

OBJDIR = output
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
DOCSOBJDIR = $(OBJDIR)/docs
ASSRCDIR = asm
CCSRCDIR = cc
LDSRCDIR = lds
CCGENDIR = cc-gen
INCLUDESGENDIR = includes-gen
CCGENSCRIPTSDIR = scripts/gen-cc
TMPDIR = tmp

VBBOXDISK = /Volumes/DATA/VirtualBox\ VMs/osdev-vms/osdev/rawdisk0.raw
QEMUDISK  = $(OBJDIR)/qemu-hda
TESTQEMUDISK  = $(OBJDIR)/qemu-test-hda
TOSDBIMG = $(OBJDIR)/tosdb.img

AS64SRCS = $(shell find $(ASSRCDIR) -type f -name \*64.S)
CC64SRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.c)
CC64TESTSRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.test.c)

CCXXSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.c)
CCXXTESTSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.test.c)

LDSRCS = $(shell find $(LDSRCDIR) -type f -name \*.ld)

UTILSSRCS = $(shell find $(UTILSSRCDIR) -type f -name \*.c)

CCGENSCRIPTS = $(shell find $(CCGENSCRIPTSDIR) -type f -name \*.sh)
GENCCSRCS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCGENDIR)/%.c,$(CCGENSCRIPTS))

ASOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.o,$(ASSRCS))

ASTESTOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.test.o,$(ASSRCS))

CC64OBJS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.o,$(CC64SRCS))
CC64OBJS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.o,$(CCXXSRCS))
CC64GENOBJS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCOBJDIR)/%.cc-gen.x86_64.o,$(CCGENSCRIPTS))

CC64ASMOUTS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.s,$(CC64SRCS))
CC64ASMOUTS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.s,$(CCXXSRCS))
CC64GENASMOUTS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCOBJDIR)/%.cc-gen.x86_64.s,$(CCGENSCRIPTS))

CC64TESTOBJS = $(patsubst $(CCSRCDIR)/%.64.test.c,$(CCOBJDIR)/%.64.test.o,$(CC64TESTSRCS))
CC64TESTOBJS += $(patsubst $(CCSRCDIR)/%.xx.test.c,$(CCOBJDIR)/%.xx_64.test.o,$(CCXXTESTSRCS))

DOCSFILES += $(CC64SRCS) $(CCXXSRCS)
DOCSFILES += $(shell find $(INCLUDESDIR) -type f -name \*.h)

FONTSRC = https://www.zap.org.au/projects/console-fonts-distributed/psftx-debian-9.4/Lat15-Terminus24x12.psf
FONTOBJ = $(OBJDIR)/font.o

OBJS = $(ASOBJS) $(CC64OBJS) $(FONTOBJ) $(CC64ASMOUTS)
TESTOBJS= $(ASTESTOBJS) $(CC64TESTOBJS)

MKDIRSDONE = .mkdirsdone

EFIDISKTOOL = $(OBJDIR)/efi_disk.bin
EFIBOOTFILE = $(OBJDIR)/BOOTX64.EFI

PROGS = $(OBJDIR)/stage3.bin.pack

TESTPROGS = $(OBJDIR)/stage3.test.bin

.PHONY: all clean depend $(SUBDIRS) $(CC64GENOBJS) asm bear
.PRECIOUS:

qemu-without-analyzer:
	CCXXEXTRAFLAGS= make -j $(shell nproc) -C utils 
	CCXXEXTRAFLAGS= make -j $(shell nproc) -C efi 
	CCXXEXTRAFLAGS= make -j 1 -f Makefile-np 
	CCXXEXTRAFLAGS= make -j $(shell nproc) -f Makefile qemu-internal

qemu: 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -C utils 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -C efi 
	CCXXEXTRAFLAGS=-fanalyzer make -j 1 -f Makefile-np 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -f Makefile qemu-internal

qemu-internal: $(QEMUDISK)

qemu-pxe: $(PROGS)
	output/pxeconfgen.bin -k stage3.bin.pack -kp $(OBJDIR)/stage3.bin.pack -o $(OBJDIR)/pxeconf.bson

qemu-test: $(TESTQEMUDISK)

virtualbox: $(VBBOXDISK)

asm:
	CCXXEXTRAFLAGS=-fanalyzer make -j 1 -f Makefile-np asm
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -f Makefile asm-internal

asm-internal: $(CC64ASMOUTS)

bear:
	bear --append -- make qemu
	bear --append -- make -C tests

test: qemu-test
	scripts/osx-hacks/qemu-hda-test.sh

gendirs:
	mkdir -p $(CCGENDIR) $(INCLUDESGENDIR) $(ASOBJDIR) $(CCOBJDIR) $(DOCSOBJDIR) $(TMPDIR)
	find $(CCSRCDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	make -C efi gendirs
	make -C tests gendirs
	make -C utils gendirs

$(OBJDIR)/docs: $(DOCSCONF) $(DOCSFILES)
	$(DOCSGEN) $(DOCSCONF)
	find output/docs/html/ -name "*.html"|sed 's-output/docs/html-https://turnstoneos.com-' > output/docs/html/sitemap.txt
	touch $(OBJDIR)/docs

$(VBBOXDISK): $(MKDIRSDONE) $(CC64GENOBJS) $(PROGS)  $(TOSDBIMG)
	$(EFIDISKTOOL) $(VBBOXDISK) $(EFIBOOTFILE) $(OBJDIR)/stage3.bin $(TOSDBIMG)

$(QEMUDISK): $(MKDIRSDONE) $(CC64GENOBJS) $(PROGS) 
	$(EFIDISKTOOL) $(QEMUDISK) $(EFIBOOTFILE) $(OBJDIR)/stage3.bin.pack $(TOSDBIMG)

$(TESTQEMUDISK): $(TESTDISK)
	$(EFIDISKTOOL) $(QEMUDISK) $(EFIBOOTFILE) $(OBJDIR)/stage3.test.bin $(TOSDBIMG)

$(MKDIRSDONE):
	mkdir -p $(CCGENDIR) $(ASOBJDIR) $(CCOBJDIR)
	touch $(MKDIRSDONE)

$(OBJDIR)/stage3.bin: $(OBJDIR)/linker.bin $(LDSRCDIR)/stage3.ld $(CC64OBJS) $(CC64GENOBJS) $(FONTOBJ)
	$(OBJDIR)/linker.bin --trim -o $@ -M $@.map -T $(filter-out $<,$^)

$(TOSDBIMG): $(OBJDIR)/generatelinkerdb.bin $(CC64OBJS) $(CC64GENOBJS) $(FONTOBJ)
	$(OBJDIR)/generatelinkerdb.bin -o $@ $(CC64OBJS) $(CC64GENOBJS) $(FONTOBJ)

$(OBJDIR)/stage3.bin.pack: $(OBJDIR)/stage3.bin
	$(OBJDIR)/zpack.bin c $^ $@

$(OBJDIR)/stage3.test.bin: $(OBJDIR)/linker.bin $(LDSRCDIR)/stage3.ld $(CC64OBJS) $(CC64GENOBJS) $(CC64TESTOBJS) $(FONTOBJ)
	$(OBJDIR)/linker.bin --test-section --trim -o $@ -M $@.map -T $(filter-out $<,$^)

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.test.o: $(CCSRCDIR)/%.xx.test.c
	$(CC64) $(CC64FLAGS) $(CXXTESTFLAGS) -o $@ $<
	
$(CCOBJDIR)/%.64.s: $(CCSRCDIR)/%.64.c
	$(CC64) $(CC64FLAGS) -S -o $@ $<

$(CCOBJDIR)/%.xx_64.s: $(CCSRCDIR)/%.xx.c
	$(CC64) $(CC64FLAGS) -S -o $@ $<
	
$(CCOBJDIR)/%.64.test.s: $(CCSRCDIR)/%.64.test.c
	$(CC64) $(CC64FLAGS) -S $(CXXTESTFLAGS) -o $@ $<

$(ASOBJDIR)/%64.o: $(ASSRCDIR)/%64.S
	$(CC64) $(CC64FLAGS) -o $@ $^

$(ASOBJDIR)/%64.test.o: $(ASSRCDIR)/%64.S
	$(CC64) $(CC64FLAGS) $(CXXTESTFLAGS) -o $@ $^

$(CCOBJDIR)/cpu/interrupt.64.o: $(CCSRCDIR)/cpu/interrupt.64.c
	$(CC64) $(CC64INTFLAGS) -o $@ $<
	
$(CCOBJDIR)/cpu/interrupt.64.s: $(CCSRCDIR)/cpu/interrupt.64.c
	$(CC64) $(CC64INTFLAGS) -S -o $@ $<

$(CCOBJDIR)/%.cc-gen.x86_64.o: $(CCGENDIR)/%.c
	$(CC64) $(CC64INTFLAGS) -o $@ $<

$(SUBDIRS):
	$(MAKE) -j $(nproc) -C $@

$(FONTOBJ):
	curl -sL -o $(OBJDIR)/font.psf $(FONTSRC)
	echo -n turnstone.kernel.hw.video > $(OBJDIR)/font.module_name.txt
	$(OBJCOPY) -O elf64-x86-64 -B i386 -I binary --rename-section .data=.rodata.font --redefine-sym _binary_output_font_psf_start=font_data_start --redefine-sym _binary_output_font_psf_end=font_data_end --redefine-sym _binary_output_font_psf_size=font_data_size --add-section .___module___=$(OBJDIR)/font.module_name.txt --add-symbol ___module___=.___module___:turnstone.kernel.hw.video $(OBJDIR)/font.psf $@
	rm -f $(OBJDIR)/font.module_name.txt

clean:
	rm -fr $(DOCSOBJDIR)/*
	if [ -d $(OBJDIR) ]; then find $(OBJDIR) -type f -delete; fi
	rm -f .depend*
	if [ -d $(CCGENDIR) ]; then find $(CCGENDIR) -type f -delete; fi
	if [ -d $(INCLUDESGENDIR) ]; then find $(INCLUDESGENDIR) -type f -delete; fi
	rm -f $(MKDIRSDONE)
	rm -f compile_commands.json

cleandirs:
	rm -fr $(CCGENDIR) $(INCLUDESGENDIR) $(OBJDIR)

print-%: ; @echo $* = $($*)

depend: .depend64

.depend64: $(CC64SRCS) $(CCXXSRCS) $(CC64TESTSRCS)
	scripts/create-cc-deps.sh "$(CC64) $(CC64FLAGS) -D___DEPEND_ANALYSIS -MM" "$^" > .depend64
	$(SEDNOBAK) 's/xx.o:/xx_64.o:/g' .depend64

-include .depend64
