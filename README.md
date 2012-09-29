Compact Kernel Version : 1.2.6
For Samsung GT-S6102 : Galaxy Y DUOS

Introduction:

This kernel source code repository has all my files downloaded from various links and research in XDA site to build a kernel for my phone and a lot of help from manoranjan2050. It includes tools to building the kernel, compress the kernel image (zImage, RamDisk), also menuconfig to config kernel, extract boot.img to get zImage & RamDisk.

Still uploading the rest of files.. all files are not done yet. 

Folders Description:

arm-2009q3 is cross compiler used to build android kernel.

ncurses-5.9 is required to enablemenuconfig option which helps you config kernel with a menu instead of editing manually .config files.. even though everyone says menuconfig is better, i prefer to edit .config file directly using text editor.

source is the folder which has files required to compile the kernel or name itself is self explanatory i guess.

bootimgpack contains all the necessary files to compress kernel(zImage) and ramdisk(ramdisk.gz). remember you need to put these 2 files of yours under this folder.

original is having all the files i have modified in source folder. There is a Readme file inside that folder which explains things more.

README.md is the file which has the contents you are reading!

and finally thanks to manoranjan2050, kurotsugi, savie and Maroc-os. All of them have taught me lot of things. and i am greatful for them. thanks guys.
