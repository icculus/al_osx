
# Tweak to your liking.

OPTIMIZE := true
USE_CCACHE := false
USE_IBM_COMPILER := false
SUPPORT_AL_EXT_VORBIS := true
SUPPORT_ALC_ENUMERATION_EXT := true
SUPPORT_AL_EXT_VECTOR_UNIT := true
SUPPORT_AL_EXT_BUFFER_OFFSET := true


# ----------------------------------------------------------------------------
# Don't touch this stuff unless you know what you're doing.

LIB := openal.dylib

ifeq ($(strip $(USE_IBM_COMPILER)),true)
  COMPILER := xlc
  COMPILERFLAGS := -qaltivec -qchars=signed -ma -qpic -qro -qroconst -qthreaded -qcpluscmt -qfullpath
  EXTRALIBS += /opt/ibmcmp/vac/6.0/lib/libxl.a
  ifeq ($(strip $(OPTIMIZE)),true)
    OPTIMIZATIONS := -O5 -D_NDEBUG=1 -Q -qhot -qignerrno -qlibansi
  else
    OPTIMIZATIONS := -O0
  endif
else
  COMPILER := gcc
  COMPILERFLAGS := -Wall -faltivec -fasm -force_cpusubtype_ALL -fsigned-chars -fPIC
  ifeq ($(strip $(OPTIMIZE)),true)
    OPTIMIZATIONS := -O3 -D_NDEBUG=1 -falign-loops=16 -ffast-math -fexpensive-optimizations -fno-common -fno-math-errno
  else
    OPTIMIZATIONS := -O0
  endif
endif

ifeq ($(strip $(USE_CCACHE)),true)
  CC := ccache $(COMPILER)
else
  CC := $(COMPILER)
endif

CFLAGS := -g $(OPTIMIZATIONS) $(COMPILERFLAGS) -D_REENTRANT -c -I../include/AL -I. -DMACOSX=1
LDFLAGS := -g -fno-common -dynamiclib -framework CoreAudio -framework CoreServices -compatibility_version 1.0.0 -install_name @executable_path/openal.dylib

SRCS := alContext.c \
		alBuffer.c \
        alEnvironment.c \
        alExtension.c \
        alListener.c \
        alSoftware.c \
        alSource.c \
        alState.c \
		ALut.c

ifeq ($(strip $(SUPPORT_AL_EXT_VECTOR_UNIT)),true)
    CFLAGS += -DSUPPORTS_AL_EXT_VECTOR_UNIT=1
endif

ifeq ($(strip $(SUPPORT_ALC_ENUMERATION_EXT)),true)
    CFLAGS += -DSUPPORTS_ALC_ENUMERATION_EXT=1
endif

ifeq ($(strip $(SUPPORT_AL_EXT_BUFFER_OFFSET)),true)
    CFLAGS += -DSUPPORTS_AL_EXT_BUFFER_OFFSET=1
endif

ifeq ($(strip $(SUPPORT_AL_EXT_VORBIS)),true)
    OGG_DIR := AL_EXT_vorbis/libogg-1.0-optimized
	OGG_SRC := $(OGG_DIR)/src
	OGG_INC := $(OGG_DIR)/include
    VORBIS_DIR := AL_EXT_vorbis/libvorbis-1.0-optimized
	VORBIS_SRC := $(VORBIS_DIR)/lib
	VORBIS_INC := $(VORBIS_DIR)/include
    CFLAGS += -I$(OGG_INC) -I$(VORBIS_INC) -DSUPPORTS_AL_EXT_VORBIS=1
    SRCS += $(OGG_SRC)/bitwise.c \
			$(OGG_SRC)/framing.c \
            $(VORBIS_SRC)/analysis.c \
            $(VORBIS_SRC)/bitrate.c \
            $(VORBIS_SRC)/block.c \
            $(VORBIS_SRC)/codebook.c \
            $(VORBIS_SRC)/envelope.c \
            $(VORBIS_SRC)/floor0.c \
            $(VORBIS_SRC)/floor1.c \
            $(VORBIS_SRC)/info.c \
            $(VORBIS_SRC)/lpc.c \
            $(VORBIS_SRC)/lsp.c \
            $(VORBIS_SRC)/mapping0.c \
            $(VORBIS_SRC)/mdct.c \
            $(VORBIS_SRC)/psy.c \
            $(VORBIS_SRC)/registry.c \
            $(VORBIS_SRC)/res0.c \
            $(VORBIS_SRC)/sharedbook.c \
            $(VORBIS_SRC)/smallft.c \
            $(VORBIS_SRC)/synthesis.c \
            $(VORBIS_SRC)/vorbisfile.c \
            $(VORBIS_SRC)/window.c \
            AL_EXT_vorbis/al_ext_vorbis.c
endif

OBJS := $(SRCS:.c=.o)

all: $(LIB)

clean:
	rm -f $(OBJS)
	rm -f $(LIB)

%.o : %.c
	$(CC) -o $@ $(CFLAGS) $^

$(LIB) : $(OBJS)
	gcc -o $@ $(LDFLAGS) $^ $(EXTRALIBS)

# end of makefile ...

