#!/bin/bash

SRC_PATH="`dirname $0`/../.."

cd $SRC_PATH

DEFCONF_ARM=`find arch/arm/configs/ -name sprd_\*_defconfig -printf "%f\n"`
DEFCONF_ARM64=`find arch/arm64/configs/ -name sprd_\*_defconfig -printf "%f\n"`

export ARCH=arm
for def in $DEFCONF_ARM;do
	if [ -f arch/arm/configs/$def ]; then
		if  make $def ; then
			if ! diff .config arch/arm/configs/$def; then
				echo "ERROR: arm defconfig $def miss order"
				if [ "$1" != "dry" ]; then
					cp -v .config arch/arm/configs/$def
					echo "arm defconfig $def updated"
				fi
			else
				echo "arm defconfig $def equals"
			fi
		else
			echo "ERROR: make defconfig $def failed"
			exit 1
		fi
	fi
done

export ARCH=arm64
for def in $DEFCONF_ARM64;do
	if [ -f arch/arm64/configs/$def ]; then
		if  make $def ; then
			if ! diff .config arch/arm64/configs/$def; then
				echo "ERROR: arm64 defconfig $def miss order"
				if [ "$1" != "dry" ]; then
					cp -v .config arch/arm64/configs/$def
					echo "arm64 defconfig $def updated"
				fi
			else
				echo "arm64 defconfig $def equals"
			fi
		else
			echo "ERROR: make defconfig $def failed"
			exit 1
		fi
	fi
done
