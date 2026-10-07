#!/bin/sh
# Capture raw frames from the IMX708 on J2 (run on the board).
#   stream.sh [width height] [sensor test pattern 0-4] [frames]
# Pattern 0 = live image, 1 = colour bars. Frames land in /root/cap.raw
# (packed RAW12, all frames back to back); CSI-2 status is printed mid-run.
W=${1:-2304}
H=${2:-1296}
TP=${3:-1}
N=${4:-8}
M="media-ctl -d /dev/media0"
SENSOR=$($M -e "imx708 2-001a")
BRIDGE=$($M -e "cv1800-vi-bridge")

$M -V "\"imx708 2-001a\":0 [fmt:SRGGB10_1X10/${W}x${H}]" || exit 1
CODE=$($M --get-v4l2 '"imx708 2-001a":0' | sed -n 's/.*fmt:\([A-Z0-9_]*\)\/.*/\1/p')
$M -V "\"cv1800-csi2\":0 [fmt:$CODE/${W}x${H}]" || exit 1
$M -V "\"cv1800-vi-bridge\":0 [fmt:$CODE/${W}x${H}]" || exit 1
case $CODE in
	SBGGR*) PIX=pBCC ;; SGBRG*) PIX=pGCC ;; SGRBG*) PIX=pgCC ;; *) PIX=pRCC ;;
esac
v4l2-ctl -d "$SENSOR" -c test_pattern="$TP"
v4l2-ctl -d "$BRIDGE" -c test_pattern=0
v4l2-ctl -d /dev/video0 --set-fmt-video width="$W",height="$H",pixelformat=$PIX || exit 1
echo "sensor $CODE ${W}x${H} -> $PIX, pattern $TP"

timeout 20 v4l2-ctl -d /dev/video0 --stream-mmap=4 --stream-skip=3 \
	--stream-count="$N" --stream-to=/root/cap.raw --verbose &
sleep 3
cat /sys/kernel/debug/a0c2000.csi/status
wait
ls -l /root/cap.raw
