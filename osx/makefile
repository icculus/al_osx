
# Tweak the below options to your liking.
#
# Set OPTIMIZE to "true" for better performance, "false" if you are debugging
#  the library. End-users will want to set this to "true".
#
# Mac users with G5's should set the USE_G5_OPCODES option to "true".
#  This will use a faster, more precise square root calculation and use
#  Altivec in places we wouldn't if we needed to check for availability, but
#  the shared library will crash on a G4 or older CPU. We set a few generic
#  compiler flags to encourage it to build code tuned for the G5, too.
#
# Mac users with G4's should set the USE_G4_OPCODES option to "true".
#  This will use Altivec in places we wouldn't if we needed to check for
#  availability, but the shared library will crash on a G3 or older CPU.
#
# Please note that if you don't set the USE_x_OPCODES makefile options, we'll
#  still try to do the right thing, including detecting Altivec support and
#  using it when it makes sense to do so...USE_G4_OPCODES just allows us to
#  use _more_ Altivec, since we can guarantee your processor has it without
#  the expense of looking for it.
#
# If you don't already know what USE_CCACHE does, set it to "false".
#
# If you don't already know what USE_VBAP does, set it to "false".
#
# If you don't already know what USE_IBM_COMPILER does, set it to "false".
#
# If you don't already know what the various SUPPORT_* things do,
#  leave them alone.

OPTIMIZE := false
USE_G4_OPCODES := false
USE_G5_OPCODES := false
USE_CCACHE := true
USE_IBM_COMPILER := false
USE_VBAP := false
SUPPORT_ALC_ENUMERATION_EXT := true
SUPPORT_ALC_EXT_CAPTURE := false
SUPPORT_ALC_EXT_SPEAKER_ATTRS := false
SUPPORT_AL_EXT_VORBIS := true
SUPPORT_AL_EXT_MP3 := true
SUPPORT_AL_EXT_VECTOR_UNIT := true
SUPPORT_AL_EXT_BUFFER_OFFSET := true
SUPPORT_AL_EXT_FLOAT32 := true

# ----------------------------------------------------------------------------
# Don't touch this stuff unless you know what you're doing.

LIB := openal.dylib

ifeq ($(strip $(USE_G5_OPCODES)),true)
  USE_G4_OPCODES := true
endif

ifeq ($(strip $(USE_IBM_COMPILER)),true)
  COMPILER := xlc
  COMPILERFLAGS := -qaltivec -qchars=signed -ma -qpic -qro -qroconst -qthreaded -qcpluscmt -qfullpath
  EXTRALIBS += /opt/ibmcmp/vac/6.0/lib/libxl.a
  ifeq ($(strip $(OPTIMIZE)),true)
    OPTIMIZATIONS := -O5 -D_NDEBUG=1 -Q -qhot -qignerrno -qlibansi
    ifeq ($(strip $(USE_G5_OPCODES),true)
      $(error fill in ibm compiler G5-specific optimizations.)
    endif
  else
    OPTIMIZATIONS := -O0
  endif
else
  COMPILER := gcc
  COMPILERFLAGS := -Wall -Werror -faltivec -fasm -force_cpusubtype_ALL -fsigned-chars -fPIC
  ifeq ($(strip $(OPTIMIZE)),true)
    OPTIMIZATIONS := -O3 -D_NDEBUG=1 -falign-loops=16 -ffast-math -fexpensive-optimizations -fno-math-errno
    ifeq ($(strip $(USE_G5_OPCODES)),true)
      OPTIMIZATIONS += -mpowerpc-gpopt -mpowerpc64 -mcpu=970 -march=970 -DFORCE_STD_SQRT=1
    endif
  else
    OPTIMIZATIONS := -O0
  endif
endif

ifeq ($(strip $(USE_G4_OPCODES)),true)
  OPTIMIZATIONS += -DFORCE_ALTIVEC=1
endif

ifeq ($(strip $(USE_CCACHE)),true)
  CC := ccache $(COMPILER)
else
  CC := $(COMPILER)
endif

CFLAGS := -g -fno-common $(OPTIMIZATIONS) $(COMPILERFLAGS) -D_REENTRANT -c -I../include/AL -I. -DMACOSX=1
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

ifeq ($(strip $(USE_VBAP)),true)
    CFLAGS += -DUSE_VBAP=1
    SRCS += alVBAP.c
endif

ifeq ($(strip $(SUPPORT_ALC_ENUMERATION_EXT)),true)
    CFLAGS += -DSUPPORTS_ALC_ENUMERATION_EXT=1
endif

ifeq ($(strip $(SUPPORT_ALC_EXT_CAPTURE)),true)
    CFLAGS += -DSUPPORTS_ALC_EXT_CAPTURE=1
endif

ifeq ($(strip $(SUPPORT_ALC_EXT_SPEAKER_ATTRS)),true)
    CFLAGS += -DSUPPORTS_ALC_EXT_SPEAKER_ATTRS=1
    ifneq ($(strip $(USE_VBAP)),true)
        $(error ALC_EXT_speaker_attrs requires VBAP support.)
    endif
endif

ifeq ($(strip $(SUPPORT_AL_EXT_VECTOR_UNIT)),true)
    CFLAGS += -DSUPPORTS_AL_EXT_VECTOR_UNIT=1
endif

ifeq ($(strip $(SUPPORT_AL_EXT_BUFFER_OFFSET)),true)
    CFLAGS += -DSUPPORTS_AL_EXT_BUFFER_OFFSET=1
endif

ifeq ($(strip $(SUPPORT_AL_EXT_FLOAT32)),true)
    CFLAGS += -DSUPPORTS_AL_EXT_FLOAT32=1
endif

ifeq ($(strip $(SUPPORT_AL_EXT_VORBIS)),true)
    OGG_DIR := AL_EXT_vorbis/libogg-1.0-optimized
	OGG_SRC := $(OGG_DIR)/src
	OGG_INC := $(OGG_DIR)/include
    VORBIS_DIR := AL_EXT_vorbis/libvorbis-1.0-optimized
	VORBIS_SRC := $(VORBIS_DIR)/lib
	VORBIS_INC := $(VORBIS_DIR)/include
    CFLAGS += -DSUPPORTS_AL_EXT_VORBIS=1 -I$(OGG_INC) -I$(VORBIS_INC)
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

ifeq ($(strip $(SUPPORT_AL_EXT_MP3)),true)
    MPGLIB_DIR := AL_EXT_mp3/mpglib
    CFLAGS += -DSUPPORTS_AL_EXT_MP3=1 -I$(MPGLIB_DIR)
    SRCS += $(MPGLIB_DIR)/dct64_i386.c \
			$(MPGLIB_DIR)/decode_i386.c \
			$(MPGLIB_DIR)/interface.c \
			$(MPGLIB_DIR)/layer1.c \
			$(MPGLIB_DIR)/layer2.c \
			$(MPGLIB_DIR)/layer3.c \
			$(MPGLIB_DIR)/mpglib_common.c \
			$(MPGLIB_DIR)/tabinit.c \
            AL_EXT_mp3/al_ext_mp3.c
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
