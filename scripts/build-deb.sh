#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Build the sg200x-camera-dkms Debian package for one architecture.
#
#   scripts/build-deb.sh riscv64|arm64 <linux source tree>
#
# The Linux tree is only needed for the device tree headers the overlay
# includes. Needs cpp, dtc and dpkg-deb; without dpkg-deb on the host, it is
# run in a Debian container if docker is available. MAINTAINER overrides the
# git identity written into the package.
set -e
ARCH=${1:?usage: $0 riscv64|arm64 <linux source tree>}
KSRC=${2:?usage: $0 riscv64|arm64 <linux source tree>}
TOP=$(cd "$(dirname "$0")/.." && pwd)
VERSION=$(sed -n 's/^PACKAGE_VERSION="\(.*\)"$/\1/p' "$TOP/dkms.conf")
MAINTAINER=${MAINTAINER:-"$(git -C "$TOP" config user.name) <$(git -C "$TOP" config user.email)>"}
case $ARCH in
	riscv64) DTSDEF= ;;
	arm64) DTSDEF=-DSG2000_ARM64 ;;
	*) echo "unknown architecture $ARCH" >&2; exit 1 ;;
esac

OUT=$TOP/out
ROOT=$OUT/$ARCH
SRC=$ROOT/usr/src/sg200x-camera-$VERSION
DOC=$ROOT/usr/share/doc/sg200x-camera-dkms
rm -rf "$ROOT"
mkdir -p "$SRC" "$DOC" "$ROOT/DEBIAN" "$ROOT/usr/lib/sg200x-camera"

# What DKMS builds from
cp "$TOP/dkms.conf" "$TOP/Kbuild" "$TOP/Makefile" "$SRC/"
cp -r "$TOP/src" "$SRC/"
rm -rf "$SRC/src/linux/patches"

# The overlays, for this architecture's interrupt controller. The postinst
# copies them into /boot, which dpkg cannot manage on Armbian's FAT partition.
for dtso in "$TOP"/dts/*.dtso; do
	name=$(basename "$dtso" .dtso)
	cpp -nostdinc -undef -D__DTS__ $DTSDEF -x assembler-with-cpp \
		-I "$KSRC/include" -I "$KSRC/arch/riscv/boot/dts/sophgo" -I "$TOP/dts" "$dtso" |
		dtc -@ -q -I dts -O dtb -o "$ROOT/usr/lib/sg200x-camera/$name.dtbo" -
done

cp "$TOP/README.md" "$TOP/packaging/copyright" "$DOC/"
for s in postinst prerm postrm; do
	sed "s/@VERSION@/$VERSION/" "$TOP/packaging/$s" > "$ROOT/DEBIAN/$s"
	chmod 755 "$ROOT/DEBIAN/$s"
done
sed -e "s/@VERSION@/$VERSION/" -e "s/@ARCH@/$ARCH/" -e "s|@MAINTAINER@|$MAINTAINER|" \
	"$TOP/packaging/control" > "$ROOT/DEBIAN/control"

DEB=sg200x-camera-dkms_${VERSION}_$ARCH.deb
if command -v dpkg-deb >/dev/null; then
	dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$OUT/$DEB" >/dev/null
else
	docker run --rm -u "$(id -u):$(id -g)" -v "$OUT":/out debian:trixie \
		dpkg-deb --root-owner-group -Zxz --build "/out/$ARCH" "/out/$DEB" >/dev/null
fi
echo "$OUT/$DEB"
