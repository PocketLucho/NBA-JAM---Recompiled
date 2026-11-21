emu2 - DOS emulator by dmsc, used to run Midway's LOAD2.EXE (the image packer)
https://github.com/dmsc/emu2   (GNU General Public License v2, see LICENSE)

The release packages carry a static Linux x86-64 build of emu2 at commit 9d8698d
with ../emu2-ems.patch applied (EMS memory, needed by LOAD2.EXE). Its complete
source is in emu2-src.tar.gz next to the binary.

To build it yourself:
    git clone https://github.com/dmsc/emu2 && cd emu2
    git checkout 9d8698d && git apply ../emu2-ems.patch
    make LDFLAGS=-static
