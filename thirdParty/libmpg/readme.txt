The mpg123 library v1.14.3
http://www.mpg123.de/

/thridParty/libmpg/ contains the basic source code to compile
a generic version of the library.

/bin/libmpg123.dll and /lib/libmpg123.lib is pre-compiled using
the Visual Studio solution provided by mpg123.

However, the compilation setting is somehow compilated for optimized
assembly version, since it need an exteral tools called yasm:
http://yasm.tortall.net/Download.html