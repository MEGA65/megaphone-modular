all:	tools/bomtool bin65/unicode-font-test.prg $(FONTS)

LINUX_BINARIES=	src/telephony/linux/provision \
		src/telephony/linux/import \
		src/telephony/linux/export \
		src/telephony/linux/thread \
		src/telephony/linux/search

CC65=cc65 -t c64
CL65=cl65 -t c64

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
			src/telephony/slab.c

HDR_TELEPHONY_COMMON=	src/telephony/records.h \
			src/telephony/contacts.h \
			src/telephony/index.h \
			src/telephony/buffers.h \
			src/telephony/search.h \
			src/telephony/sms.h \
			src/telephony/slab.h

SRC_TELEPHONY_COMMON_LINUX=	src/telephony/linux/hal.c

HDR_TELEPHONY_COMMON_LINUX=	src/telephony/linux/includes.h

HDR_PATH_LINUX=	-Isrc/telephony/linux -Isrc/telephony

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

bin65/unicode-font-test.prg:	src/telephony/unicode-font-test.c src/telephony/attr_tables.c src/telephony/screen.c
	mkdir -p bin65
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/telephony/unicode-font-test.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/telephony/screen.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/telephony/attr_tables.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/mega65-libc/src/shres.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/mega65-libc/src/memory.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/mega65-libc/src/hal.c
	$(CL65) -o bin65/unicode-font-test.prg -Iinclude -Isrc/mega65-libc/include src/telephony/unicode-font-test.s src/telephony/screen.s src/telephony/attr_tables.s src/mega65-libc/src/shres.s src/mega65-libc/src/cc65/shres_asm.s src/mega65-libc/src/memory.s src/mega65-libc/src/cc65/memory_asm.s src/mega65-libc/src/hal.s

test:	$(LINUX_BINARIES)
	src/telephony/linux/provision 5 10
	python3 src/telephony/sms-stim.py -o stim.txt 5 10
	src/telephony/linux/import stim.txt
	src/telephony/linux/search PHONE/CONTACT0.D81 PHONE/IDXALL-0.D81 "Nicole"
	src/telephony/linux/search PHONE/CONTACT0.D81 PHONE/IDXALL-0.D81 "99"
	src/telephony/linux/export export.txt
	cat export.txt
