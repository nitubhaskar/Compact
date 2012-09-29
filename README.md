Compact Kernel Version : 1.2.6
For Samsung GT-S6102 : Galaxy Y DUOS

Introduction:

This kernel source code repository has all my files downloaded from various links and research in XDA site to build a kernel for my phone and a lot of help from manoranjan2050. It includes tools to building the kernel, compress the kernel image (zImage, RamDisk), also menuconfig to config kernel, extract boot.img to get zImage & RamDisk.


Still uploading the rest of files.. all files are not done yet. 

Folders Description:

"arm-2009q3" is cross compiler used to build android kernel.

"ncurses-5.9" is required to enablemenuconfig option which helps you config kernel with a menu instead of editing manually .config files.. even though everyone says menuconfig is better, i prefer to edit .config file directly using text editor.

"source" is the folder which has files required to compile the kernel or name itself is self explanatory i guess.

"bootimgpack" contains all the necessary files to compress kernel(zImage) and ramdisk(ramdisk.gz). remember you need to put these 2 files of yours under this folder.

"original" is having all the files i have modified in source folder. There is a Readme file inside that folder which explains things more.

".config working backup" has my .config file.. just for backup purpose.

"README.md" is the file which has the contents you are reading!



COMMANDS used: from building kernel to packing boot.img

use the commands without QUOTES "" in terminal.

Note: first 3 commands should be entered inside this folder or path
/"your path to kernel"/common/
For Ex. mine is here /home/com/kernel/source/common/
So my terminal should be showing the above when i type "pwd" command. (pwd means PresentWorkingDirectory).

last 2 commands should be entered inside the bootimg folder.


1. "make clean" --> its better to do this. so that you will have a fresh files to build kernel and no old files compiled before will be used.

2. "make bcm21553_torino_02_defconfig" --> this sets everything to default settings as provided by samsung source.

3. "make" --> this is used to compile the kernel. this does the compilation in only single process.
alternate command to speed up the compilation process do "make -j(x)"
Note: remove () from -j option.. and x means the number of threads to use.
if you have new cpu or powerful cpu in PC then use the -j option.
for Ex. make -j4 to run 4 process together to compile. if you have quad core then use it.

4. "mv -i /"your path to kernel"/common/arch/arm/boot/zImage /"your path to bootimg folder"/" --> use this to move zImage created after finishing compilation which takes around 20 minutes. if you used make only. -i is for interactive mode. or you can just go to that folder by gui and copy paste it to bootimg folder.
For Ex. this is what i use in my system. 
mv -i /home/com/kernel/common/arch/arm/boot/zImage /home/com/Desktop/bootimg

5. "md5sum zImage" --> is md5sum of zImage file which is required in boot.img for galaxy y series. If you leave this. It wont boot.
you will get something like
d78f52b755b388381d6207c13d8a46ab zImage

6. "./mkbootimg --kernel zImage --ramdisk ramdisk.gz --base 0x81600000 --kernelMD5 d78f52b755b388381d6207c13d8a46ab -o boot.img" --> You need to add the md5sum obtained in 5th command in this command.

Now you have yourself a boot.img file in the same folder which you can flash through Odin software by making a tar compression of boot.img or make a flashable zip file for cwm.


and finally thanks to manoranjan2050, kurotsugi, savie and Maroc-os. All of them have taught me lot of things. i am greatful for them. thanks guys.
