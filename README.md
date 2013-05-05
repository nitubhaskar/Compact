Compact Kernel Version : 1.3
For Samsung GT-S5512 : Galaxy Y PRO DUOS

Introduction:
~~~~~~~~~~~~~

This kernel source code repository has all my files downloaded from various links and research in XDA site to build a kernel for 5512 phone and a lot of help from manoranjan2050. It includes tools to building the kernel, compress the kernel image (zImage, RamDisk), also menuconfig to config kernel, extract boot.img to get zImage & RamDisk.


Folders Description:

You can get the cross compiler(Toolchain) used to build this kernel from here. Visit http://www.codesourcery.com/, download and install it.

"arm-2009q3" is cross compiler used to build android kernel. Or You can also get the Toolchain from here. Visit http://www.codesourcery.com/, download and install it(hard way).

"ncurses-5.9" under "Usefull_Tools" is required to enable menuconfig option which helps you config kernel with a menu instead of editing manually .config files.. even though everyone says menuconfig is better, i prefer to edit .config file directly using text editor.
You can also get ncurses from its official page from here. link--> http://ftp.gnu.org/pub/gnu/ncurses/

"Compact-Kernel" is the folder which has files required to compile the kernel or name itself is self explanatory i guess.

"mkbootimg_src" under "Usefull_Tools" contains all the necessary files to compress kernel(zImage) and ramdisk(ramdisk.gz). remember you need to put these 2 files of yours under this folder.

"Samsung-Stock" is having all the files i have modified in source folder. There is a Readme file inside that folder which explains things more.

"README.md" is the file which has the contents you are reading!



COMMANDS used: from building kernel to packing boot.img

Note: 

1. use the commands without QUOTES "" in terminal.

2. open folder "common" inside source folder, there is a file called "Makefile". open it with text editor and find this line using search option "/opt/toolchains/arm-eabi-4.4.3/bin/arm-eabi-". replace the line with this "/<your path to toolchain folder>/arm-2009q3/bin/arm-none-eabi-".
For Ex. in mine the line has this after replacing.
/home/com/kernel/arm-2009q3/bin/arm-none-eabi-

3. first 3 commands should be entered inside this folder or path
/<your path to kernel folder>/common/
For Ex. mine is here /home/com/kernel/source/common/
So my terminal should be showing the above when i type "pwd" command. (pwd means PresentWorkingDirectory).

4. last 2 commands should be entered inside the bootimg folder.

Now your system environment is setup and ready to build the kernel.. Follow the below commands to build the kernel.


1. "make clean" --> its better to do this. so that you will have a fresh files to build kernel and no old files compiled before will be used.

2. "make bcm21553_luisa_ds_defconfig" --> this sets everything to default settings as provided by samsung source.

3. "make" --> this is used to compile the kernel. this does the compilation in only single process.
alternate command to speed up the compilation process do "make -j(x)"
Note: remove () from -j option.. and x means the number of threads to use.
if you have new cpu or powerful cpu in PC then use the -j option.
for Ex. make -j4 to run 4 threads together to compile. if you have quad core then use it.

4. "mv -i /"your path to kernel"/common/arch/arm/boot/zImage /"your path to bootimg folder"/" --> use this to move zImage created after finishing compilation which takes around 20 minutes. if you used make only. -i is for interactive mode. or you can just go to that folder by gui and copy paste it to bootimg folder.
For Ex. this is what i use in my system. 
mv -i /home/com/kernel/common/arch/arm/boot/zImage /home/com/Desktop/bootimg

5. "md5sum zImage" --> is md5sum of zImage file which is required in boot.img for galaxy y series. If you leave this. It wont boot.
you will get something like
d78f52b755b388381d6207c13d8a46ab zImage

6. "./mkbootimg --kernel zImage --ramdisk ramdisk.gz --base 0x81600000 --kernelMD5 d78f52b755b388381d6207c13d8a46ab -o boot.img" --> You need to add the md5sum obtained in 5th command in this command.

Now you have yourself a boot.img file in the same folder which you can flash through Odin software by making a tar compression of boot.img or make a flashable zip file for cwm.


and finally thanks to manoranjan2050, kurotsugi, savie, irfanbagus and Maroc-os. All of them have taught me lot of things. i am grateful for them. thanks guys.
