CFLAGS ?= -O3 -march=native
#CFLAGS ?= -O3 -march=native -fsanitize=undefined
#WFLAGS = -pedantic -Wall -Wextra -Wno-unused -Wno-overlength-strings -Wno-unused-parameter
#CFLAGS ?= -O3 -march=native -std=c17 $(WFLAGS)
#CFLAGS ?= -O3 -march=native #-Mnobuiltin -Wno-unused-variable
#CFLAGS ?= -O3 -Wall -Wextra -Wno-unused -Wno-unused-parameter
LDLIBS += -ljpeg -ltiff -lpng -lz -lfftw3f -lm# -lubsan


OBJ = src/iio.o src/fancy_image.o
BIN = plambda vecov veco vecoh morsi downsa upsa ntiply censust dither qauto \
      qeasy homwarp synflow backflow flowinv nnint bdint amle simpois ghisto \
      contihist fontu imprintf pview viewflow flowarrows palette ransac blur \
      srmatch tiffu siftu crop lrcat tbcat fftshift bmms registration imflip \
      fft dct dht flambda fancy_crop fancy_downsa autotrim iion mediator     \
      redim colormatch eucdist nonmaxsup gntiply idump warp heatd imhalve    \
      ppsmooth mdither mdither2 rpctk getbands pixdump geomedian #carve

BIN := $(addprefix bin/,$(BIN))

default: $(BIN) bin/cpu_term bin/rpcflip_term

bin/%  : src/%.o $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)


DISABLE_HDF5 = 1
ifndef DISABLE_HDF5
# comment the following two lines to disable hdf5 support
LDLIBS += $(shell pkg-config hdf5 --libs --silence-errors || echo -lhdf5)
src/iio.o: CPPFLAGS+= -DI_CAN_HAS_LIBHDF5 `pkg-config hdf5 --cflags 2>/dev/null`
endif




# END OF CORE PART, THE REST OF THIS FILE IS NOT ESSENTIAL

# unit test (end-to-end)
ifeq ($(MAKECMDGOALS),test)
LDLIBS := $(filter-out -lfftw3f,$(LDLIBS))
endif
test: bin/plambda bin/imprintf
	echo $(MAKECMDGOALS)
	bin/plambda zero:512x512 "randg randg randl randg randg 5 njoin" \
	| bin/plambda "split rot del hypot" | bin/imprintf "%s%e%r%i%a\n" \
	| grep -q 3775241.440161.730120.0037888911.8794


# build tutorial
tutorial: default
	cd doc/tutorial/f && env PATH=../../../bin:$(PATH) $(SHELL) build.sh

# build manpages
manpages: default
	cd doc && $(SHELL) rebuild_manpages.sh



# exotic targets, not built by default
# FTR: interactive tools, require X11 or freeglut
# MSC: "misc" tools, or those requiring GSL

OBJ_FTR = src/iio.o src/ftr/ftr.o src/ftr/egm96.o src/ftr/fancy_image.o

BIN_FTR = $(shell cat src/ftr/TARGETS)
BIN_MSC = $(shell cat src/misc/TARGETS)

BIN_FTR := $(addprefix bin/,$(BIN_FTR))
BIN_MSC := $(addprefix bin/,$(BIN_MSC))

LDLIBS_FTR = $(LDLIBS) -lGL -lglut
LDLIBS_FTR = $(LDLIBS) -lX11
LDLIBS_MSC = $(LDLIBS) -lgsl -lgslcblas

OBJ_ALL = $(OBJ) $(OBJ_FTR)
BIN_ALL = $(BIN) $(BIN_FTR) $(BIN_MSC)

full  : default ftr misc
ftr   : $(BIN_FTR)
misc  : $(BIN_MSC)


bin/% : src/ftr/%.o $(OBJ_FTR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS_FTR)

bin/% : src/misc/%.o $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS_MSC)




# single, fat, busybox-like executable
BINOBJ = $(BIN:bin/%=src/%.o) #$(BIN_FTR:bin/%=src/ftr/%.o) src/ftr/ftr.o
L = -lfftw3f -lpng -ltiff -ljpeg -llzma -lz -lm -ljbig $(LDLIBS) -lm -lpthread
#L = -lfftw3f -lpng -ltiff -ljpeg -llzma -lz -lm -ljbig -lwebp -lpthread -lzstd -lX11 -lxcb -ldl
bin/im.static : src/im.o $(BINOBJ) $(OBJ) src/misc/overflow.o
	$(CC) $(LDFLAGS) -static -Wl,--allow-multiple-definition -o $@ $^ $L

BINOBJ2 = $(BIN:bin/%=src/%.o) $(BIN_FTR:bin/%=src/ftr/%.o) src/ftr/ftr.o
L2 = $(LDLIBS) $(LDLIBS_FTR)
bin/im : src/im.o $(BINOBJ2) $(OBJ) src/misc/overflow.o
	$(CC) $(LDFLAGS) -Wl,--allow-multiple-definition -o $@ $^ $(L2)


# some ftr executables, but compiled for the terminal backend
OBJ_FTR_TERM = src/ftr/ftr_term.o $(filter-out src/ftr/ftr.o,$(OBJ_FTR))
bin/%_term : src/ftr/%.o $(OBJ_FTR_TERM)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# bureaucracy
clean: ; @$(RM) $(BIN_ALL) bin/im src/*.o src/ftr/*.o src/misc/*.o
.PHONY: default full ftr misc clean tutorial manpages
.PRECIOUS: %.o


# hack (compatibility flags for old compilers)
#
# The following conditional statement appends "-std=gnu99" to CFLAGS when the
# compiler does not define __STDC_VERSION__.  The idea is that many older
# compilers are able to compile standard C when given that option.
# This hack seems to work for all versions of gcc, clang and icc.
ifeq (,$(shell $(CC) $(CFLAGS) -dM -E -< /dev/null | grep __STDC_VERSION_))
CFLAGS := $(CFLAGS) -std=gnu99
endif




# non-standard file dependences
# (this is needed because some .c files include other .c files directly)
DIRS = src src/ftr src/misc
.deps.mk:;for i in $(DIRS);do $(CC) -MM $$i/*.c|sed "\:^[^ ]:s:^:$$i/:g";done>$@
-include .deps.mk
