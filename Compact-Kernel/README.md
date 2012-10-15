Compact Kernel:
~~~~~~~~~~~~~~~
For Samsung GT-S6102 : Galaxy Y DUOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Introduction
~~~~~~~~~~~~

Folders description:
~~~~~~~~~~~~~~~~~~~~


"common" and "modules" folders: these are the folder which has files required to compile the kernel. These two folders are obtained from samsung source

"MerrukTechnology_Output": This is the Output `Directory` for all compiled files.
Compiled Files :
	* 1. Kernel Image
		The Kernel image file `zImage` on the root of this Output directory "./".
	* 2. Kernel Modules
		All Kernel modules `files.ko` will be automatically copied to "./system/lib/modules/".


"Tools": This `Directory` is an add-ons gives you a simple way to Compress/Decompress all kernels with different Compression types.

	You can use the tools to install the environment at first time with ./Install.sh [parameter]. `(Decompression)`
	Also you can use it to compress the files after you generate files in `./Boot` directory {InitRamFS}. `(Compression)`


Compiling:
~~~~~~~~~

For compiling Merruk Technology kernel open the command prompt `(CONSOLE)` and see this :

<table>
  <tr>
	<th>Description</th><th>Meaning</th><th>Command</th><th>Parameter</th>
  </tr>
  <tr>
	<td>New Compile/Update Existing Kernel</td><td>First Compile or Only Modded Codes</td><td>Kernel_Make</td><td>-MT</td>
  </tr>
  <tr>
	<td>Specific 'CONFIG_FILE' Compile</td><td>Must be in <b>/arch/arm/configs/</b></td><td>Kernel_Make</td><td>-CF bcm21553_torino_02_defconfig</td>
  </tr>
  <tr>
	<td>Clean Build (0 files already built)</td><td>Like first time, clean files and build</td><td>Kernel_Make</td><td>-CL</td>
  </tr>
  <tr>
	<td>Specify number of prosessors</td><td>Script guesses that automatically</td><td>Kernel_Make</td><td>-CPU [Number of cores] (all cores by default)</td>
  </tr>
</table>


Preparing the environment:
~~~~~~~~~~~~~~~~~~~~~~~~~~

On first use of the `Tools` directory you need to set up the environment :

Usage:

	cd ./Tools

	./Install.sh [Parameter]

Parameters:
<dl>
	<dt>merruk</dt>
	<dd>Use Merruk Technology RamDisk</dd>
	<dt>stock</dt>
	<dd>Use Samsung RamDisk</dd>
</dl>


Kernel compression:
~~~~~~~~~~~~~~~~~~~

You can simply call this tool after you made changes in the 'Boot' directory `(RamFS)` :

Usage:

	cd ./Tools

	./Compress.sh [Parameter]

Parameters:
<dl>
	<dt>merruk</dt>
	<dd>Use Merruk Technology kernel</dd>
	<dt>stock</dt>
	<dd>Use Samsung kernel</dd>
</dl>


Output files:
~~~~~~~~~~~~~

Your compressed kernel will be built as:

<table>
	<tr>
		<td>PDA.[parameter].tar</td><td>Odin file, search for a compatible version</td>
	</tr>
	<tr>
		<td>Kernel.[parameter].Boot.img</td><td>Raw file, flash with 'dd' command</td>
	</tr>
</table>
