# JeremyOS
A very basic operating system created as part of a university course using x86 ASM and C. Tested using Bochs, hence the included bochs shortcut file.

After building the .img and .iso files using "make", use "copytestfiles.bat" to copy the contents of "TestDirStructure" into the .img and .iso files. This will provide the OS with a file structure that can be traversed and files that can be read. Without doing this the OS will have no file structure, and it is not currently equipped to create files/directories internally.
