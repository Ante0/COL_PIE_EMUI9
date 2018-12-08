#!/bin/bash
#make sure you added toolchain to bash.rc
#example
#export AARCH64_COMPILR=$HOME/android/kernel/aarch64-linux-android-4.9/bin
#export CROSS_COMPILE=aarch64-linux-android-
#export PATH=$PATH:$AARCH64_COMPILR
make ARCH=arm64 O=out/ merge_kirin970_defconfig
make ARCH=arm64 O=out/ -j8
