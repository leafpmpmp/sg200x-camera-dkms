#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Refresh the private copies of kernel code in src/linux/ from a Linux tree.
#
#   scripts/update-linux-copies.sh <linux source tree>
#
# The imx708 module needs the CCS PLL calculator with the change from the
# IMX708 v2 series (src/linux/patches/). The sensor modules need the V4L2
# CCI register helpers, which kernels only build when an in-tree driver
# selects them. The helpers are compiled into each module; their exports are
# dropped so they never collide with the kernel's own ccs-pll or v4l2-cci
# modules. ov5647.c is mainline's driver, for kernels that do not build it.
set -e
KSRC=${1:?usage: $0 <linux source tree>}
TOP=$(cd "$(dirname "$0")/.." && pwd)
OUT=$TOP/src/linux

KVER=$(make -s -C "$KSRC" kernelversion 2>/dev/null) ||
	KVER=$(awk '/^VERSION/{v=$3} /^PATCHLEVEL/{p=$3} /^SUBLEVEL/{s=$3} END{print v"."p"."s}' "$KSRC/Makefile")

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cp "$KSRC/drivers/media/i2c/ccs-pll.c" "$KSRC/drivers/media/i2c/ccs-pll.h" \
	"$KSRC/drivers/media/v4l2-core/v4l2-cci.c" "$KSRC/drivers/media/i2c/ov5647.c" "$TMP/"
for p in "$OUT"/patches/*.patch; do
	(cd "$TMP" && patch -s -p4 < "$p")
done

mkdir -p "$OUT/ccs"
cp "$KSRC/drivers/media/i2c/ccs/ccs-regs.h" "$OUT/ccs/"
cp "$TMP/ccs-pll.h" "$OUT/"
for f in ccs-pll.c v4l2-cci.c; do
	grep -vE '^(EXPORT_SYMBOL(_GPL)?|MODULE_(LICENSE|AUTHOR|DESCRIPTION))\(' "$TMP/$f" |
		sed "1a /* Private copy from Linux $KVER for the camera modules: nothing exported */" \
		> "$OUT/$f"
done
sed "1a /* Copy of mainline's driver from Linux $KVER, unchanged */" "$TMP/ov5647.c" \
	> "$OUT/ov5647.c"
echo "src/linux refreshed from Linux $KVER"
