#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Capture YUV frames through the ISP (run on the board, as root).
#
#   stream-yuv.sh [width height] [frames]
#
# Frames land back to back in $OUT (default cap.yuv) as $FMT (NV21, the
# default, or NV12). PATTERN=1 feeds the bridge's colour bars to the ISP
# instead of the sensor image. RED and BLUE set the white balance gains
# (1024 = 1.0), BLACK the black level (12-bit units), GAMMA 0 or 1 the sRGB
# curve. T sets the capture timeout in seconds.
M="media-ctl -d /dev/media0"
OUT=${OUT:-cap.yuv}
FMT=${FMT:-NV21}

# The sensor is whatever feeds the CSI-2 receiver
SENSOR=$($M -p | sed -n 's/^.*<- "\([^"]*\)":0 \[ENABLED.*$/\1/p' | grep -v '^cv1800' | head -1)
[ -n "$SENSOR" ] || { echo "no sensor in the media graph" >&2; exit 1; }
fmt() { $M --get-v4l2 "\"$SENSOR\":0" | sed -n "s/.*fmt:\([A-Z0-9_]*\)\/\([0-9]*\)x\([0-9]*\).*/\\$1/p"; }

W=${1:-$(fmt 2)}
H=${2:-$(fmt 3)}
N=${3:-8}

$M -V "\"$SENSOR\":0 [fmt:$(fmt 1)/${W}x${H}]" || exit 1
CODE=$(fmt 1) W=$(fmt 2) H=$(fmt 3)
$M -V "\"cv1800-csi2\":0 [fmt:$CODE/${W}x${H}]" || exit 1
$M -V "\"cv1800-vi-bridge\":0 [fmt:$CODE/${W}x${H}]" || exit 1

YUV=$($M -e cv1800-vi-yuv)
[ -n "$YUV" ] || { echo "no cv1800-vi-yuv node: the driver has no ISP path" >&2; exit 1; }
v4l2-ctl -d "$($M -e cv1800-vi-bridge)" -c test_pattern="${PATTERN:-0}"
v4l2-ctl -d "$YUV" -c red_balance="${RED:-1024}",blue_balance="${BLUE:-1024}" \
	-c gamma="${GAMMA:-1}",black_level="${BLACK:-0}" || exit 1
v4l2-ctl -d "$YUV" --set-fmt-video width="$W",height="$H",pixelformat="$FMT" || exit 1
echo "$SENSOR: $CODE ${W}x${H} -> ISP -> $FMT, bridge pattern ${PATTERN:-0}"

timeout "${T:-20}" v4l2-ctl -d "$YUV" --stream-mmap=4 --stream-skip=3 \
	--stream-count="$N" --stream-to="$OUT" --verbose
# Frame, drop and ISP counts of the stream
cat /sys/kernel/debug/a000000.video-input/status 2>/dev/null
ls -l "$OUT"
