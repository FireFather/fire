UNAME = $(shell uname)

ifeq ($(COMP),mingw)
	EXE = fire.exe
else
	EXE = fire
endif

PGOBENCH = ./$(EXE) bench 14

OBJS =
	OBJS += bench.o bitboard.o chrono.o \
	evaluate.o hash.o main.o movegen.o \
	movepick.o nnue.o perft.o position.o \
	search.o thread.o uci.o util.o zobrist.o \
	
optimize = yes
debug = no
sanitize = none
bits = 64
prefetch = no
popcnt = no
sse = no
mmx = no
sse2 = no
ssse3 = no
sse41 = no
avx2 = no
pext = no

ifeq ($(ARCH),x86-64-sse41)
	arch = x86_64
	bits = 64
	prefetch = yes
	popcnt = yes
	sse = yes
	sse2 = yes
	ssse3 = yes
	sse41 = yes
endif

ifeq ($(ARCH),x86-64-avx2)
	arch = x86_64
	bits = 64
	popcnt = yes
	sse = yes
	sse2 = yes
	ssse3 = yes
	sse41 = yes
	avx2 = yes
endif

ifeq ($(ARCH),x86-64-pext)
	arch = x86_64
	bits = 64
	popcnt = yes
	sse = yes
	sse2 = yes
	ssse3 = yes
	sse41 = yes
	avx2 = yes
	pext = yes
endif

ifeq ($(sse),yes)
	prefetch = yes
endif

CXXFLAGS += -Wcast-qual -fno-exceptions $(EXTRACXXFLAGS)
DEPENDFLAGS += -std=c++17
LDFLAGS += $(EXTRALDFLAGS)
CC = gcc

ifeq ($(COMP),)
	COMP=gcc
endif

ifeq ($(COMP),gcc)
	comp=gcc
	CXX=g++
	CXXFLAGS += -pedantic -Wextra -Wshadow -m$(bits)
endif

ifeq ($(COMP),mingw)
	comp=mingw

	ifeq ($(UNAME),Linux)
		ifeq ($(bits),64)
			ifeq ($(shell which x86_64-w64-mingw32-c++-posix),)
				CXX=x86_64-w64-mingw32-c++
			else
				CXX=x86_64-w64-mingw32-c++-posix
			endif
		else
			ifeq ($(shell which i686-w64-mingw32-c++-posix),)
				CXX=i686-w64-mingw32-c++
			else
				CXX=i686-w64-mingw32-c++-posix
			endif
		endif
	else
		CXX=g++
	endif

	CXXFLAGS += -Wextra -Wshadow
	LDFLAGS += -static
endif

profile_prepare = gcc-profile-prepare
profile_make = gcc-profile-make
profile_use = gcc-profile-use
profile_clean = gcc-profile-clean

ifdef COMPILER
	COMPCXX=$(COMPILER)
endif

ifdef COMPCXX
	CXX=$(COMPCXX)
endif

ifneq ($(comp),mingw)
	ifneq ($(arch),armv7)
		ifneq ($(UNAME),Haiku)
			LDFLAGS += -lpthread
		endif
	endif
endif

ifeq ($(debug),no)
	CXXFLAGS += -DNDEBUG
else
	CXXFLAGS += -g
endif

ifeq ($(optimize),yes)
	CXXFLAGS += -O3
endif

ifeq ($(bits),64)
	CXXFLAGS += -DIS_64_BIT
endif

ifeq ($(prefetch),yes)
	ifeq ($(sse),yes)
		CXXFLAGS += -msse
		DEPENDFLAGS += -msse
	endif
else
	CXXFLAGS += -DNO_PREFETCH
endif

ifeq ($(popcnt),yes)
	ifeq ($(bits),64)
		CXXFLAGS += -msse3 -mpopcnt -DUSE_POPCNT
	else
		CXXFLAGS += -mpopcnt -DUSE_POPCNT
	endif
endif

ifeq ($(sse2),yes)
	CXXFLAGS += -DUSE_SSE2
	ifeq ($(comp),$(filter $(comp),gcc clang mingw))
		CXXFLAGS += -msse2
	endif
endif

ifeq ($(ssse3),yes)
	CXXFLAGS += -DUSE_SSSE3
	ifeq ($(comp),$(filter $(comp),gcc clang mingw))
		CXXFLAGS += -mssse3
	endif
endif

ifeq ($(sse41),yes)
	CXXFLAGS += -DUSE_SSE41
	ifeq ($(comp),$(filter $(comp),gcc clang mingw))
		CXXFLAGS += -msse4.1
	endif
endif

ifeq ($(avx2),yes)
	CXXFLAGS += -DUSE_AVX2
	ifeq ($(comp),$(filter $(comp),gcc clang mingw))
		CXXFLAGS += -mavx2
	endif
endif

ifeq ($(pext),yes)
	CXXFLAGS += -DUSE_PEXT
	ifeq ($(comp),$(filter $(comp),gcc clang mingw))
		CXXFLAGS += -mbmi -mbmi2
	endif
endif

ifeq ($(comp),gcc)
	ifeq ($(optimize),yes)
	ifeq ($(debug),no)
		CXXFLAGS += -flto
		LDFLAGS += $(CXXFLAGS)
	endif
	endif
endif

ifeq ($(comp),mingw)
	ifeq ($(UNAME),Linux)
	ifeq ($(optimize),yes)
	ifeq ($(debug),no)
		CXXFLAGS += -flto
		LDFLAGS += $(CXXFLAGS)
	endif
	endif
	endif
endif

CFLAGS := $(CXXFLAGS)
CXXFLAGS += -fno-rtti -std=c++20

help:
	@echo ""
	@echo "To compile fire, type: "
	@echo "make target ARCH=arch [COMP=compiler] [COMPCXX=cxx]"
	@echo ""
	@echo "Supported targets:"
	@echo "build                   > Standard build"
	@echo "profile-build           > PGO build"
	@echo "strip                   > Strip executable"
	@echo "clean                   > Clean up"
	@echo "gcc-profile-clean       > Clean up after PGO build"
	@echo ""
	@echo "Supported architectures:"
	@echo "x86-64-sse41            > x86 64-bit with sse41 support"
	@echo "x86-64-avx2             > x86 64-bit with avx2 support"	
	@echo "x86-64-pext             > x86 64-bit with pext support"
	@echo ""
	@echo "Supported compilers:"
	@echo "gcc                     > Gnu compiler (default)"
	@echo "mingw                   > Gnu compiler with MinGW under Windows"
	@echo ""
	@echo "make build ARCH=x86-64-sse41"	
	@echo "make build ARCH=x86-64-avx2"	
	@echo "make build ARCH=x86-64-pext"
	@echo ""
	@echo "make profile-build ARCH=x86-64-sse41"	
	@echo "make profile-build ARCH=x86-64-avx2"
	@echo "make profile-build ARCH=x86-64-pext"	
	@echo ""

.PHONY: build profile-build
build:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) config-sanity
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) all

profile-build:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) config-sanity
	@echo ""
	@echo "preparing for profile build..."
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_prepare)
	@echo ""
	@echo "building executable for benchmark..."
	$(MAKE) -B ARCH=$(ARCH) COMP=$(COMP) $(profile_make)
	@echo ""
	@echo "running benchmark for pgo-build..."
	$(PGOBENCH)
	@echo ""
	@echo "building final executable..."
	$(MAKE) -B ARCH=$(ARCH) COMP=$(COMP) $(profile_use)
	@echo ""
	@echo "deleting profile data..."
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) $(profile_clean)
	$(MAKE) strip

strip:
	strip $(EXE)

install:
	-mkdir -p -m 755 $(BINDIR)
	-cp $(EXE) $(BINDIR)
	-strip $(BINDIR)/$(EXE)

clean:
	$(RM) *.o .depend *.gcda *.map *.txt

default:
	help

all: $(EXE) .depend

config-sanity:
	@echo ""
	@echo "Config:"
	@echo "debug: '$(debug)'"
	@echo "optimize: '$(optimize)'"
	@echo "arch: '$(arch)'"
	@echo "bits: '$(bits)'"
	@echo "prefetch: '$(prefetch)'"
	@echo "popcnt: '$(popcnt)'"
	@echo "sse: '$(sse)'"
	@echo "sse2: '$(sse2)'"	
	@echo "ssse3: '$(ssse3)'"
	@echo "sse41: '$(sse41)'"
	@echo "avx2: '$(avx2)'"
	@echo "pext: '$(pext)'"
	@echo ""
	@echo "Compiler:"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo ""
	@echo "Testing config sanity. If this fails, try 'make help' ..."
	@echo ""
	@test "$(debug)" = "yes" || test "$(debug)" = "no"
	@test "$(optimize)" = "yes" || test "$(optimize)" = "no"
	@test "$(arch)" = "any" || test "$(arch)" = "x86_64"
	@test "$(bits)" = "32" || test "$(bits)" = "64"
	@test "$(prefetch)" = "yes" || test "$(prefetch)" = "no"
	@test "$(popcnt)" = "yes" || test "$(popcnt)" = "no"
	@test "$(sse)" = "yes" || test "$(sse)" = "no"
	@test "$(mmx)" = "yes" || test "$(mmx)" = "no"
	@test "$(sse2)" = "yes" || test "$(sse2)" = "no"
	@test "$(ssse3)" = "yes" || test "$(ssse3)" = "no"
	@test "$(sse41)" = "yes" || test "$(sse41)" = "no"
	@test "$(avx2)" = "yes" || test "$(avx2)" = "no"
	@test "$(pext)" = "yes" || test "$(pext)" = "no"
	@test "$(comp)" = "gcc" || test "$(comp)" = "mingw"

$(EXE): $(OBJS) $(COBJS)
	$(CXX) -o $@ $(OBJS) $(COBJS) $(LDFLAGS)

$(COBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

gcc-profile-prepare:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) gcc-profile-clean

gcc-profile-make:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) \
	EXTRACXXFLAGS='-fprofile-generate' \
	EXTRALDFLAGS='-lgcov' \
	all

gcc-profile-use:
	$(MAKE) ARCH=$(ARCH) COMP=$(COMP) \
	EXTRACXXFLAGS='-fprofile-use -fno-peel-loops -fno-tracer -fprofile-correction' \
	EXTRALDFLAGS='-lgcov -Wl,-Map,h.map' \
	all

gcc-profile-clean:
	@rm -rf *.txt
	@rm -rf *.map
	@rm -rf *.gcda
	@rm -rf *.o
	
.depend:
	-@$(CXX) $(DEPENDFLAGS) -MM $(OBJS:.o=.cpp) $(COBJS:.o=.c) > $@ 2> /dev/null

-include .depend
