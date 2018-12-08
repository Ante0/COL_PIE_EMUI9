#!/bin/bash
VERS="$(make kernelversion)"
./mkbootimg --kernel out/arch/arm64/boot/Image.gz --base 0x0 --cmdline "loglevel=4 initcall_debug=n page_tracker=on unmovable_isolate1=2:192M,3:224M,4:256M printktimer=0xfff0a000,0x534,0x538 androidboot.selinux=enforcing buildvariant=user" --tags_offset 0x07A00000 --kernel_offset 0x00080000 --ramdisk_offset 0x07c00000 --os_version 8.0.0 --os_patch_level $1  --output BKL_KERNEL_$1_$VERS.img
echo "kernel image created: BKL_KERNEL_$1_$VERS.img"
cp out/arch/arm64/boot/Image.gz releases/Image_$VERS.gz
echo "Image.gz copied to releases/Image_$VERS.gz"
