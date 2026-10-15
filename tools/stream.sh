#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Capture raw frames from the camera on J2 (run on the board, as root).
#
#   stream.sh [width height] [test pattern] [frames]
#
# Pattern 0 is the live image, 1 the sensor's colour bars. Frames land back
# to back in $OUT (default cap.raw) as packed RAW12; the sensor may pick the
# nearest size it supports. T sets the capture timeout in seconds.
M="media-ctl -d /dev/media0"
OUT=${OUT:-cap.raw}

# The sensor is whatever feeds the CSI-2 receiver
SENSOR=$($M -p | sed -n 's/^.*<- "\([^"]*\)":0 \[ENABLED.*$/\1/p' | grep -v '^cv1800' | head -1)
[ -n "$SENSOR" ] || { echo "no sensor in the media graph" >&2; exit 1; }
fmt() { $M --get-v4l2 "\"$SENSOR\":0" | sed -n "s/.*fmt:\([A-Z0-9_]*\)\/\([0-9]*\)x\([0-9]*\).*/\\$1/p"; }

W=${1:-$(fmt 2)}
H=${2:-$(fmt 3)}
TP=${3:-1}
N=${4:-8}

$M -V "\"$SENSOR\":0 [fmt:$(fmt 1)/${W}x${H}]" || exit 1
CODE=$(fmt 1) W=$(fmt 2) H=$(fmt 3)
$M -V "\"cv1800-csi2\":0 [fmt:$CODE/${W}x${H}]" || exit 1
$M -V "\"cv1800-vi-bridge\":0 [fmt:$CODE/${W}x${H}]" || exit 1
case $CODE in
	SBGGR*) PIX=pBCC ;; SGBRG*) PIX=pGCC ;; SGRBG*) PIX=pgCC ;; *) PIX=pRCC ;;
esac
v4l2-ctl -d "$($M -e "$SENSOR")" -c test_pattern="$TP"
v4l2-ctl -d "$($M -e cv1800-vi-bridge)" -c test_pattern=0
v4l2-ctl -d /dev/video0 --set-fmt-video width="$W",height="$H",pixelformat=$PIX || exit 1
echo "$SENSOR: $CODE ${W}x${H} -> $PIX, pattern $TP"

timeout "${T:-20}" v4l2-ctl -d /dev/video0 --stream-mmap=4 --stream-skip=3 \
	--stream-count="$N" --stream-to="$OUT" --verbose &
sleep 3
cat /sys/kernel/debug/a0c2000.csi/status 2>/dev/null
wait
ls -l "$OUT"
