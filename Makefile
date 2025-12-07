PROGRAMS := foneinit fonemain

all:	tools/bomtool $(PROGRAMS:%=bin65/%.llvm.prg) $(FONTS)

LINUX_BINARIES=	src/telephony/linux/provision \
		src/telephony/linux/import \
		src/telephony/linux/export \
		src/telephony/linux/thread \
		src/telephony/linux/search

CC65=cc65 -t c64
CL65=cl65 -t c64

COPT_M65=	-Iinclude	-Isrc/telephony/mega65 -Isrc/mega65-libc/include

COMPILER=llvm
COMPILER_PATH=/usr/local/bin
CC=   $(COMPILER_PATH)/mos-c64-clang -mcpu=mos45gs02 -Iinclude -Isrc/telephony/mega65 -Isrc/mega65-libc/include -DLLVM -fno-unroll-loops -ffunction-sections -fdata-sections -mllvm -inline-threshold=0 -fvisibility=hidden -Oz -Wall -Wextra -Wtype-limits

# Uncomment to include stacktraces on calls to fail()
CC+=	-g -finstrument-functions -DWITH_BACKTRACE

LD=   $(COMPILER_PATH)/ld.lld
CL=   $(COMPILER_PATH)/mos-c64-clang -DLLVM -mcpu=mos45gs02
HELPERS=        src/helper-llvm.c

LDFLAGS += -Wl,-T,src/telephony/asserts.ld
# Produce reproducer tar when required for assisting with debugging
LDFLAGS += -Wl,--reproduce=repro.tar


As the MEGA65 libc has also advanced considerably since I last worked on GRAZE, I also reworked how I pull that in:

M65LIBC_INC=-I $(SRCDIR)/mega65-libc/include
M65LIBC_SRCS=$(wildcard $(SRCDIR)/mega65-libc/src/*.c) $(wildcard $(SRCDIR)/mega65-libc/src/$(COMPILER)/*.c) $(wildcard $(SRCDIR)/mega65-libc/src/$(COMPILER)/*.s)
CL65+=-I include $(M65LIBC_INC)


FONTS=fonts/twemoji/twemoji.MRF \
	fonts/noto/NotoEmoji-VariableFont_wght.ttf.MRF \
	fonts/noto/NotoSans-VariableFont_wdth,wght.ttf.MRF \
	fonts/nokia-pixel-large/nokia-pixel-large.otf.MRF

fonts/noto/NotoColorEmoji-Regular.ttf:
	echo "Read fonts/noto/README.txt"

fonts/twemoji/twemoji.MRF:
	python3 tools/twemoji2mega65font.py fonts/twemoji/assets/svg/ fonts/twemoji/twemoji.MRF

%.otf.MRF:	%.otf tools/showglyph
	tools/showglyph $<

%.ttf.MRF:	%.ttf tools/showglyph
	tools/showglyph $<

fonts:	$(FONTS)


tools/bomtool:	tools/bomtool.c tools/parts-library.c tools/parts-library.h
	gcc -Wall -o $@ tools/bomtool.c tools/parts-library.c

tools/showglyph:	tools/showglyph.c
	gcc -o tools/showglyph tools/showglyph.c -I/usr/include/freetype2 -lfreetype

tools/make-dialpad:	tools/make-dialpad.c
	gcc -o tools/make-dialpad tools/make-dialpad.c -I/usr/include/freetype2 -lfreetype

dialpad.NCM:	tools/make-dialpad
	tools/make-dialpad fonts/Orbitron/Orbitron-ExtraBold.ttf fonts/noto/NotoEmoji-VariableFont_wght.ttf "0123456789#*☎✖🙊" 

tools/gen_attr_tables:	tools/gen_attr_tables.c
	gcc -o tools/gen_attr_tables tools/gen_attr_tables.c

src/telephony/attr_tables.c:	tools/gen_attr_tables
	$< > $@

SRC_TELEPHONY_COMMON=	src/telephony/d81.c \
			src/telephony/records.c \
			src/telephony/contacts.c \
			src/telephony/sort.c \
			src/telephony/index.c \
			src/telephony/buffers.c \
			src/telephony/search.c \
			src/telephony/sms.c \
			src/telephony/loader.c \
			src/telephony/shstate.c \
			src/telephony/mountstate.c \
			src/telephony/slab.c

NATIVE_TELEPHONY_COMMON=	$(SRC_TELEPHONY_COMMON) \
			src/telephony/screen.c \
			src/telephony/dialer.c \
			src/telephony/status.c \
			src/telephony/smsscreens.c \
			src/telephony/af.c \
			src/telephony/contactscreens.c \
			src/telephony/wait_sprite.c \


OBJ_TELEPHONY_NATIVE=	src/telephony/d81.s \
			src/telephony/records.s \
			src/telephony/screen.s \
			src/telephony/contacts.s \
			src/telephony/sort.s \
			src/telephony/index.s \
			src/telephony/buffers.s \
			src/telephony/search.s \
			src/telephony/sms.s \
			src/telephony/loader.s \
			src/telephony/shstate.s \
			src/telephony/mountstate.s \
			src/telephony/smsscreens.s \
			src/telephony/contactscreens.s \
			src/telephony/af.s \
			src/telephony/slab.s \
			src/telephony/dialer.s \
			src/telephony/status.s \
			src/telephony/mega65/hal.s \
			src/telephony/mega65/hal_asm.s \
			src/telephony/helper-cc65.s \

HDR_TELEPHONY_COMMON=	src/telephony/records.h \
			src/telephony/contacts.h \
			src/telephony/index.h \
			src/telephony/buffers.h \
			src/telephony/search.h \
			src/telephony/sms.h \
			src/telephony/shstate.h \
			src/telephony/dialer.h \
			src/telephony/slab.h

OBJ_MEGA65_LIBC=	src/mega65-libc/src/shres.s \
			src/mega65-libc/src/cc65/shres_asm.s \
			src/mega65-libc/src/memory.s \
			src/mega65-libc/src/cc65/memory_asm.s \
			src/mega65-libc/src/cc65/fileio.s \
			src/mega65-libc/src/hal.s

SRC_MEGA65_LIBC_LLVM=	src/mega65-libc/src/shres.c \
			src/mega65-libc/src/llvm/shres_asm.s \
			src/mega65-libc/src/memory.c \
			src/mega65-libc/src/llvm/memory_asm.s \
			src/mega65-libc/src/llvm/fileio.s \
			src/mega65-libc/src/hal.c


SRC_TELEPHONY_COMMON_LINUX=	src/telephony/linux/hal.c

HDR_TELEPHONY_COMMON_LINUX=	src/telephony/linux/includes.h

HDR_PATH_LINUX=	-Isrc/telephony/linux -Isrc/telephony

src/telephony/wait_sprite.c:	tools/sprite_gen.c
	gcc -Wall -o tools/sprite_gen tools/sprite_gen.c
	tools/sprite_gen > src/telephony/wait_sprite.c

src/telephony/linux/provision:	src/telephony/provision.c $(SRC_TELEPHONY_COMMON) $(HDR_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX) $(HDR_TELEPHONY_COMMON_LINUX)
	gcc -Wall -g $(HDR_PATH_LINUX) -o $@ src/telephony/provision.c $(SRC_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX)

src/telephony/linux/import:	src/telephony/import.c $(SRC_TELEPHONY_COMMON) $(HDR_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX) $(HDR_TELEPHONY_COMMON_LINUX)
	gcc -Wall -g $(HDR_PATH_LINUX) -o $@ src/telephony/import.c $(SRC_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX)

src/telephony/linux/export:	src/telephony/export.c $(SRC_TELEPHONY_COMMON) $(HDR_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX) $(HDR_TELEPHONY_COMMON_LINUX)
	gcc -Wall -g $(HDR_PATH_LINUX) -o $@ src/telephony/export.c $(SRC_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX)

src/telephony/linux/search:	src/telephony/linux/search.c $(SRC_TELEPHONY_COMMON) $(HDR_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX) $(HDR_TELEPHONY_COMMON_LINUX)
	gcc -Wall -g $(HDR_PATH_LINUX) -o $@ src/telephony/linux/search.c $(SRC_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX)

src/telephony/linux/thread:	src/telephony/linux/thread.c $(SRC_TELEPHONY_COMMON) $(HDR_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX) $(HDR_TELEPHONY_COMMON_LINUX)
	gcc -Wall -g $(HDR_PATH_LINUX) -o $@ src/telephony/linux/thread.c $(SRC_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX)

src/telephony/linux/sortd81:	src/telephony/sortd81.c $(SRC_TELEPHONY_COMMON) $(HDR_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX) $(HDR_TELEPHONY_COMMON_LINUX)
	gcc -Wall -g $(HDR_PATH_LINUX) -o $@ src/telephony/sortd81.c $(SRC_TELEPHONY_COMMON) $(SRC_TELEPHONY_COMMON_LINUX)

bin65/fonesms.cc65.prg:	src/telephony/fonesms.c $(NATIVE_TELEPHONY_COMMON)
	mkdir -p bin65

	$(CC65) $(COPT_M65) src/telephony/fonesms.c
	$(CC65) $(COPT_M65) src/telephony/screen.c
	$(CC65) $(COPT_M65) src/telephony/mega65/hal.c
	$(CC65) $(COPT_M65) src/telephony/buffers.c
	$(CC65) $(COPT_M65) src/telephony/contacts.c
	$(CC65) $(COPT_M65) src/telephony/index.c
	$(CC65) $(COPT_M65) src/telephony/sort.c
	$(CC65) $(COPT_M65) src/telephony/search.c
	$(CC65) $(COPT_M65) src/telephony/sms.c
	$(CC65) $(COPT_M65) src/telephony/loader.c
	$(CC65) $(COPT_M65) src/telephony/shstate.c
	$(CC65) $(COPT_M65) src/telephony/smsscreens.c
	$(CC65) $(COPT_M65) src/telephony/d81.c
	$(CC65) $(COPT_M65) src/telephony/slab.c
	$(CC65) $(COPT_M65) src/telephony/records.c
	$(CC65) $(COPT_M65) src/telephony/attr_tables.c
	$(CC65) $(COPT_M65) src/mega65-libc/src/shres.c
	$(CC65) $(COPT_M65) src/mega65-libc/src/memory.c
	$(CC65) $(COPT_M65) src/mega65-libc/src/hal.c
	$(CL65) -o bin65/fonemain.prg -Iinclude -Isrc/mega65-libc/include src/telephony/fonemain.s src/telephony/attr_tables.s $(OBJ_TELEPHONY_NATIVE) $(OBJ_MEGA65_LIBC) 

# For backtrace support we have to compile twice: Once to generate the map file, from which we
# can generate the function list, and then a second time, where we link that in.
HELPER_SRCS=	 src/telephony/attr_tables.c src/telephony/helper-llvm.s \
		 src/telephony/mega65/hal.c src/telephony/mega65/hal_asm_llvm.s
bin65/%.llvm.prg:	src/telephony/%.c $(NATIVE_TELEPHONY_COMMON)
	mkdir -p bin65
	rm -f src/telephony/mega65/function_table.c
	echo "struct function_table function_table[]={}; const unsigned int function_table_count=0; const unsigned char __wp_regs[9];" > src/telephony/mega65/function_table.c
	$(CC) -o bin65/$*.llvm.prg -Iinclude -Isrc/mega65-libc/include $< $(HELPER_SRCS) $(NATIVE_TELEPHONY_COMMON) $(SRC_MEGA65_LIBC_LLVM) $(LDFLAGS)  -Wl,-Map,bin65/$*.map
	tools/function_table.py bin65/$*.map src/telephony/mega65/function_table.c
	$(CC) -o bin65/$*.llvm.prg -Iinclude -Isrc/mega65-libc/include $< $(HELPER_SRCS) $(NATIVE_TELEPHONY_COMMON) $(SRC_MEGA65_LIBC_LLVM) $(LDFLAGS) -Wl,-Map,bin65/$*.map
	llvm-objdump -drS --print-imm-hex bin65/$*.llvm.prg.elf >bin65/$*.llvm.dump


test:	$(LINUX_BINARIES)
	src/telephony/linux/provision 
	python3 src/telephony/sms-stim.py -o stim.txt 5 10
	src/telephony/linux/import stim.txt
	src/telephony/linux/search PHONE/CONTACT0.D81 PHONE/IDXALL-0.D81 "Nicole"
	src/telephony/linux/search PHONE/CONTACT0.D81 PHONE/IDXALL-0.D81 "99"
	src/telephony/linux/export export.txt
	cat export.txt

sdcardprep:	$(LINUX_BINARIES)
	src/telephony/linux/provision /media/paul/MEGA65FDISK
	python3 src/telephony/sms-stim.py -o stim.txt 10 100
	src/telephony/linux/import stim.txt /media/paul/MEGA65FDISK

sdbin:	bin65/foneinit.llvm.prg bin65/foneclst.llvm.prg bin65/fonesms.llvm.prg

sdpush: sdbin
	cp bin65/foneinit.llvm.prg /media/paul/MEGA65FDISK/PHONE/FONEINIT.PRG
	cp bin65/foneclst.llvm.prg /media/paul/MEGA65FDISK/PHONE/FONECLST.PRG
	cp bin65/fonesms.llvm.prg /media/paul/MEGA65FDISK/PHONE/FONESMS.PRG
	umount /media/paul/MEGA65FDISK

ftppush:	sdbin
	m65 -F
	sleep 2
	m65ftp -l /dev/ttyUSB0 -c "cd PHONE" -c "put bin65/foneinit.llvm.prg FONEINIT.PRG"
	m65ftp -l /dev/ttyUSB0 -c "cd PHONE" -c "put bin65/foneclst.llvm.prg FONECLST.PRG"
	m65ftp -l /dev/ttyUSB0 -c "cd PHONE" -c "put bin65/fonesms.llvm.prg FONESMS.PRG" -c "exit"

ftprun:	ftppush
	m65 -4 -r bin65/foneinit.llvm.prg

ftpload:	ftppush
	m65 -4 bin65/foneinit.llvm.prg
	m65 -t run
