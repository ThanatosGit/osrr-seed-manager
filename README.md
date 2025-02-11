# README

## How to use?

### Install

Download the `3dsx` file from the release page, copy it to your `3ds` folder on your SD card and use the homebrew launcher to start the application.
Alternatively, you can use the `cia` from the release page and install it via FBI.

### Application

The OSRR seed manager consists of a server part and a seed installation part.

All `zip` files in the `/osrr-seed-manager` path on your SD card are considered as a seed. You can place a zipped seed there. Do not include the title id, which means in the root of the `zip` file there should be the `romfs` folder alongside other files. Make sure that the file ends with `.zip`.

A list of all `zip` files will be displayed on the bottom screen. Use your touch controls to select a seed and confirm the modal dialogue to install it. In the current state, your input will be blocked while the seed is extracted.

Also note that in the current state, the `/luma/titles/{TITLE_ID}` folder will be deleted before the seed is extracted. Please note that this means that it is incompatible with other mods.

The server part can receive a `zip` file. This is just a handy feature to place the file into the `/osrr-seed-manager` path. It will not unzip the file directly!  
If you do not want to use this, simply copy the `zip` files via ftp or place them directly onto your SD card e.g. via your computer + SD card reader. An example on how to sent to the server can be found in this repository in the `example` folder.


## Development

Do not forget to init the submodules because this repository is using:  
- imgui-3ds: https://github.com/hax0kartik/imgui-3ds

The CI script is loosely based on what https://github.com/devkitPro/3ds-hbmenu/ did.  
For extracting `libarchive` is used: https://github.com/libarchive/libarchive/

`cia.rsf` based on https://gist.github.com/jakcron/9f9f02ffd94d98a72632

### Prerequisites

Usual stuff like `libctru`, `devkitarm`. Use a search engine how to install it on your machine.  
Everything was done with `wsl` ony my side.

TODO: Dependencies on PORTLIBS. See `dkp-pacman -Sl` output

### Built the project

Just call `make`.

### Generate cia file

Use `makerom` from https://github.com/3DSGuy/Project_CTR/releases.
There is compiled version included in the `utils` folder.

```
$DEVKITPRO/devkitARM/arm-none-eabi/bin/strip osrr-seed-manager.elf -o osrr-seed-manager_stripped.elf
./utils/makerom -f cia -o osrr-seed-manager.cia -target t -exefslogo -rsf cia.rsf -elf osrr-seed-manager_stripped.elf -icon osrr-seed-manager.smdh

```

## ToDos:
- Actually write the example to sent to the server :-)
- License for rdv logo?
- Add version to cia
- Maybe add a banner
- Speed it up somehow?!?