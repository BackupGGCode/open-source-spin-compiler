# Command Line Options #
| **option** | **Description** |
|:-----------|:----------------|
| -h | Display help for the command line options. |
| -L or -I < path > | Add a directory to the include path. |
| -o < path > | Output filename. Allows you to override the default name. |
| -b | Output will be in "binary" format. |
| -e | Output will be in "eeprom" format. |
| -c | Output only DAT sections as a dat file. |
| -d | Dump out docs mode. |
| -t | Output just the object file tree to stdout. |
| -f | Output a list of filenames for use in archiving to stdout. |
| -q | Quiet mode. Suppress banner and non-error text. |
| -v | Verbose mode. Output extra info. |
| -p | Disable the [preprocessor](Preprocessor.md). |
| -a | Use alternative [preprocessor](Preprocessor.md) rules. |
| -D < define > | Add a define to the [preprocessor](Preprocessor.md). |
| -M < size > | Set size of eeprom (up to 16777216 bytes). |