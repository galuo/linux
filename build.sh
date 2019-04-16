source /opt/Xilinx/SDK/2017.4/settings64.sh
export CROSS_COMPILE=arm-linux-gnueabihf-
make ARCH=arm UIMAGE_LOADADDR=0x8000 uImage -j4
