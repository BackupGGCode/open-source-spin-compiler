# OpenSpin's Preprocessor #
The preprocessor is on by default. You can turn it off with the **-p** [command line option](CommandLine.md).
## Supported Commands ##
  * #define FOO
    * simple macros with no arguments, basic substitution & #ifdef checking
  * #undef
  * #ifdef FOO / #ifndef FOO
  * #else / #elseifdef FOO / #elseifndef FOO
  * #endif
  * #error message
  * #warn message
  * #include "file"
## Alternative Preprocessor Mode ##
If you use the **-a** [command line option](CommandLine.md), that will turn on alternative mode for the preprocessor. The difference is in how #define and #error work.

For #define, if you just define something (e.g. #define FOO or use the -D command line option) then it will not perform substitution in the code. It will just satisfy #ifdef and similar checks.

For #error, it will exit the compiler immediately after displaying the error message.