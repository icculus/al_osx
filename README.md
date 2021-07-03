# OBSOLETE.

This was a really great OpenAL implementation in the early 2000's, but
it has not been maintained or even tested on newer Macs. Assume it
doesn't work now. Apple ships an OpenAL implementation with macOS and iOS
now, [OpenAL-Soft](https://openal-soft.org/) is an extremely robust
open source option, and [mojoAL](https://icculus.org/mojoAL/) is a single
C file OpenAL that uses SDL2 for the heavy lifting. All of these are
better options.

Original README follows.


# al_osx

A Mac OS X implementation of OpenAL.

Please visit http://www.openal.org/ for API documentation.

This implementation of the OpenAL API is _NOT_ the one from openal.org...it
was originally based on the openal.org MacOS 9 version, but is an almost total
rewrite of that codebase.

This codebase is designed around and optimized for Mac OS X and its CoreAudio
API. It does not work on MacOS 9 at all. Some notable features that set it
apart from the standard openal.org Mac implementation:

- Speed. This is currently the fastest AL implementation for OSX.
- Contains optimizations for Altivec-enabled systems, but has scalar fallbacks
  for G3 and older systems.
- Is designed with multithreading in mind...should be 100% thread safe, and
  can take advantage of multiple CPU systems internally.
- Supports more-than-two speaker configurations...for example, you can
  install an M-Audio Revolution 7.1 card and surround yourself with speakers;
  audio sources rendered through OpenAL will correctly use speakers, so that
  sounds played behind the listener will literally play behind you, too.
- Can handle multiple contexts on multiple devices, all running in parallel.
- Supports the AL_ENUMERATION_EXT extension for device enumeration.
- Supports AL_EXT_vorbis for direct playback of Ogg Vorbis audio; the Vorbis
  decoder is a highly optimized and Altivec-enabled fork of xiph.org's
  libvorbis-1.0...using this extension can be significantly faster and
  easier for the application than using a stock libvorbis and buffer queueing.
- Supports ALC_EXT_capture, for recording audio from a microphone, etc.
- Supports ALC_EXT_speaker_attrs, for fine-tuned control of physical
  loudspeakers in 3D space.
- Supports AL_EXT_float32, for easier migration from CoreAudio to OpenAL.
- Supports AL_EXT_buffer_offset, which is basically an extended form of
  AL_BYTE_LOKI, for when you have to get down and dirty at the lowlevel of
  audio playback.
- Codebase is designed to be maintainable and flexible. Adding new data
  types (as demostrated by AL_EXT_vorbis) is fairly easy.
- Supports IBM's G5 compiler.

This implementation is fairly robust, but in many ways can still be considered
a "mini-AL" library...it is enough to play UT2003 well, but some features of a
complete OpenAL library are missing, and some elements are not perfectly in
line with the 1.0 specification at this time. Bug reports and patches to make
the implementation more robust are welcome.

Please see the file LICENSE.txt for licensing information, TODO.txt for things still
to be done, and INSTALL.txt for basic installation instructions.

--ryan. (icculus@icculus.org)


External copyrights:

Bits of code were inspired by, and cut-and-pasted from Ville Pulkki's Vector
 Base Amplitude Panning (VBAP) source code. Those algorithms enable us to
 efficiently spatialize audio sources across multiple speakers. Here is the
 copyright that code falls under:

Copyright 1998 by Ville Pulkki, Helsinki University of Technology.  All
rights reserved.  

The software may be used, distributed, and included to commercial
products without any charges. When included to a commercial product,
the method "Vector Base Amplitude Panning" and its developer Ville
Pulkki must be referred to in documentation.

This software is provided "as is", and Ville Pulkki or Helsinki
University of Technology make no representations or warranties,
expressed or implied. By way of example, but not limitation, Helsinki
University of Technology or Ville Pulkki make no representations or
warranties of merchantability or fitness for any particular purpose or
that the use of the licensed software or documentation will not
infringe any third party patents, copyrights, trademarks or other
rights. The name of Ville Pulkki or Helsinki University of Technology
may not be used in advertising or publicity pertaining to distribution
of the software.

