# Introduction #

Here's the current state of the compiler.


# Details #

The solution/project files are for VS2008, and there is a Makefile for GCC. Tested on MinGW, linux, and Mac OSX so far. Thanks Steve Denson, for the Makefile and testing on linux! Thanks to David Betz for testing on Mac OSX.

PropellerCompiler.cpp/h contain the interface to the compiler. Look at openspin.cpp for an example of how to work with the interface.

openspin.exe is a command line wrapper for the compiler. You give it a spin file and it passes it through the compiler and produces a .binary file with the same base name as the passed in spin file. There are several command line options available. Run openspin.exe with no arguments to get a usage description.

As of version [r41](https://code.google.com/p/open-source-spin-compiler/source/detail?r=41), it supports a basic preprocessor. Now on by default, disable with the -p option. Thanks to Eric Smith for providing the code and helping with integrating it.

The code now successfully compiles all of the Library files shipped with PropTool as well as all of the files available in the OBEX as of August 2012.

The only binary differences are from the corrected handling of floating point numbers (which is now IEEE compliant).