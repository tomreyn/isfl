# isfl - Insyde Software UEFI firmware flashing Linux driver

This is version 0.0.05 of the (GPLv2 licensed) Linux driver required to carry out firmware upgrades on Insyde Softwares UEFI platform, using the H2OFFT-L(x64) utility on GNU Linux. The utility itself is not available here, since it does not seem to be GPL licensed.

## Compatibility

This kernel module was released a part of InsydeFlash for Linux Version 1.1c, alongside Insyde H2OFFT (Firmware Flash Tool) Version 100.00.07.21.

## Requirements

* Kernel headers for your currently running kernel, >= 2.6.36
* GNU make

## Compilation

To build the kernel module:

* Change into the directory matching your platform
* Run: `make clean`
* Run: `make`

On a successful build, the kernel module will be built:
* isfl_drv.ko (for ia32)
* isfl_drv_x64.ko (for x64)

Load it using `modprobe -v isfl_drv` or `modprobe -v isfl_drv_x64`.

## Trouble shooting

Upon loading the module, if you run into the `-1 Invalid module format` error message, this suggests you are using a module that was built on a different platform (ia32 vs. x64) or for an ABI incompatible kernel version.

## No maintenance

Please note that I (@tomreyn, you may be looking at a fork of the original repo where things may differ) have no intention of maintaining this code. I'm merely making it available here on GitHub for others to fork and hack on it, just in case it goes offline at the original location I copied this GPLv2 code from. If you have questions about this code or the H2OFFT-L(x64) software, feel free to email me: `tomreyn 47 megaglest |)07 org`.
