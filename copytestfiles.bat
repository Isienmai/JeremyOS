rem Mount the disk image file as drive y. If you
rem want to use a different drive letter, change it 
rem in the line below and in the line at the end of the file.
imdisk -a -t file -f JeremyOS.img -o rem -m y:
rem
xcopy "TestDirStructure" "y:\" /E /H
rem
imdisk -D -m y:
