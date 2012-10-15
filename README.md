Compact Kernel:
~~~~~~~~~~~~~~~
For Samsung GT-S6102 : Galaxy Y DUOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Introduction:
~~~~~~~~~~~~~

-	This kernel source code repository has included tools to simplify building the kernel, create/install environment and, eventually, compress the kernel image `(zImage, RamDisk)` after you make your modifications in the `Boot` directory.

-	When you execute the scripts you will see the verbose output for better understanding the processes.

This can be done in three steps:
<dl>
	<dt>* 1. Compiling</dt>
	<dd>	Build the kernel. There are 4 parameters to customize your build.</dd>
	<dt>* 2. Preparing the environment</dt>
	<dd>	Set up the environment and decompress the RamDisk.</dd>
	<dt>* 3. Kernel compression</dt>
	<dd>	Compress the kernel image with modifications in the boot directory or with default Merruk or Samsung files.</dd>
</dl>


Compiling:
~~~~~~~~~~

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
  <tr>
	<td>Make the torino defconfig 02</td><td>menuconfig use the actual torino</td><td>Kernel_Make</td><td>MENU</td>
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
~~~~~~
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

Now you have yourself a boot.img file in the same folder which you can flash through Odin software by making a tar compression of boot.img into boot.tar or make a flashable zip file for cwm.


and finally thanks to manoranjan2050, kurotsugi, savie and Maroc-os. All of them have taught me lot of things. i am greatful for them. thanks guys.
