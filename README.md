# sg200x-camera-dkms

Camera support for Sophgo SG200x boards on mainline Linux. Armbian builds it
into its sophgo-sg200x kernels; other kernels build it on the board with DKMS.

Tested on the Milk-V Duo S under Armbian with Linux 7.2, on both of its cores
(C906 riscv64 and Cortex-A53 arm64): a Raspberry Pi Camera Module 3 and a
Camera Module v1 on J2, and a Milk-V CAM-GC2083 on J1 (riscv64 only).

## What is in it

| Module        | What it does |
|---------------|--------------|
| `cv1800-vi`   | Capture through the VI block: raw frames from its CSI bridge as packed 12-bit Bayer, or NV21/NV12 frames from the SoC's ISP. Media-controller driver with a bridge sub-device (including the bridge's test pattern generator) and two video nodes, `cv1800-vi-raw` and `cv1800-vi-yuv`. One of them streams at a time. |
| `cv1800-csi2` | MIPI CSI-2 receiver: MAC 0 and the D-PHY, lanes taken from the device tree. |
| `imx708`      | Sony IMX708 driver from its upstream v2 series, plus the 2x2 binned 2304x1296 mode and power-off between streams (see Limitations). |
| `gc2083`      | GalaxyCore GC2083 driver (Milk-V CAM-GC2083), by Lukasz Sobala. |
| `dw9817-vcm`  | DKMS only: the Camera Module 3's focus motor driver, based on `dw9807-vcm`. Armbian's kernels add the DW9817 to `dw9807-vcm` itself. |
| `ov5647`      | DKMS only: mainline's OV5647 driver (Camera Module v1), for kernels that do not build it. |

Plus device tree overlays for the Duo S connectors:
`sg2000-camera-j2-imx708` (Camera Module 3 on J2), `sg2000-camera-j2-ov5647`
(Camera Module v1 on J2) and `sg2000-camera-j1-gc2083` (CAM-GC2083 on J1).
Enable one of them: all cameras use the same CSI-2 receiver.

## Armbian

Armbian's `sophgo-sg200x-camera` extension copies `linux/` into the kernel
tree and builds the drivers and overlays with the kernel. Do not install this
package on these kernels: DKMS modules replace the kernel's own. Enable a
camera in `/boot/armbianEnv.txt` and reboot:

    overlays=camera-j2-imx708    # or camera-j2-ov5647, camera-j1-gc2083

## Requirements

- A kernel with `CONFIG_VIDEO_CAMERA_SENSOR=y` (the kernel default), which
  brings the V4L2 sub-device API, and with a CMA pool big enough for the frame
  buffers: about 18 MB for four binned frames, 72 MB for four full-resolution
  ones. The ISP path adds three raw frames of its own (9.3 MB at 1080p).
  Armbian's sophgo-sg200x kernels reserve 128 MiB.
- The headers package of the running kernel, and `dkms`.
- A short ribbon cable. J2's I2C runs through an auto-direction level shifter
  that rings on long cables: 150 mm works, 500 mm does not.

## Install

Build the package for the board's architecture. The Linux tree only provides
the device tree headers the overlay includes:

    scripts/build-deb.sh riscv64 /path/to/linux    # or arm64

On the board, install the kernel headers first, then the package. DKMS builds
the modules and rebuilds them for every kernel installed later:

    apt install linux-headers-edge-sophgo-sg200x-riscv64   # your kernel's
    apt install ./sg200x-camera-dkms_0.2.0_riscv64.deb

Enable the overlay by adding it to `user_overlays=` in `/boot/armbianEnv.txt`,
then reboot:

    user_overlays=sg2000-camera-j2-imx708    # or sg2000-camera-j2-ov5647, sg2000-camera-j1-gc2083

`media-ctl -p` should then show the sensor, the CSI-2 receiver, the bridge
and the video node.

Without the package, `make KDIR=/path/to/kernel/build` builds the modules, and
`dkms add .` registers the tree with DKMS.

## Using it

- **Raw frames**: `tools/stream.sh [width height] [pattern] [frames]` sets up
  the pipeline for the connected sensor and captures to `cap.raw` (or `$OUT`).
  Pattern 0 is the live image, 1 the sensor's colour bars. Frames are packed
  RAW12 (the MIPI layout, lines padded to 16 bytes) with the sensor's 10 bits
  in the top bits.
- **YUV frames from the ISP**: `tools/stream-yuv.sh [width height] [frames]`
  sets up the same pipeline and captures from `cv1800-vi-yuv` to `cap.yuv`
  (or `$OUT`), as NV21 (or `FMT=NV12`). `PATTERN=1` sends the bridge's colour
  bars through the ISP instead of the sensor image. The node has four
  controls: `red_balance` and `blue_balance` (white balance gains, 1024 =
  1.0), `black_level` (12-bit units, 0 = off; 252 for the GC2083) and
  `gamma` (1 = sRGB curve, 0 = linear). Convert a frame on a PC with
  `ffmpeg -f rawvideo -pix_fmt nv21 -s 1920x1080 -i cap.yuv -frames 1 cap.png`.
  See "The ISP path" below.
- **libcamera**: its simple pipeline handler and software ISP turn the camera
  into an ordinary RGB camera for `cam`, GStreamer's `libcamerasrc`, PipeWire
  and V4L2 applications under `libcamerify`. libcamera has to know the
  `cv1800-vi` driver: `extras/libcamera/libcamera-sg200x.patch` (against
  v0.7.2) adds it, with an IMX708 tuning file. At 720p the software ISP runs
  at about 15 fps on the Cortex-A53 and 5 to 6 fps on the C906.
- **Focus**: `focus_absolute` (0-1023) on the lens sub-device, `dw9817` (or
  `dw9807` on Armbian's kernels). The lens is
  only powered while something holds that node open; libcamera does.
- **Orientation**: the Camera Module 3 overlay declares the sensor as rotated
  by 180 degrees, so libcamera flips it upright. The v1 needs no rotation.

The sensors themselves deliver:

| Sensor | Mode | Frame rate |
|--------|------|------------|
| IMX708 | 4608x2592 | 14.35 fps |
| IMX708 | 2304x1296 (binned) | 56 fps |
| OV5647 | 2592x1944 | 11.7 fps |
| OV5647 | 1920x1080 | 25 fps |
| OV5647 | 1296x972 | 24 fps |
| OV5647 | 640x480 | 62.5 fps |
| GC2083 | 1920x1080 | 29.7 fps |

## Debugging

- `/sys/kernel/debug/a000000.video-input/status`: what the CSI bridge
  measured in the incoming frames, every status bit seen since the stream
  started, and how often the bridge was reset after a frame of the wrong
  size. For the ISP path also: frames the ISP processed and dropped, stalls,
  and the reset value of the RGB to YUV matrix. After a stream the counts
  of that stream stay readable.
- `/sys/kernel/debug/a0c2000.csi/status`: receiver state, decoded data type,
  lane LP states and error counts. Writing 1 to `power` powers the receiver
  without a stream, to check that an idle sensor's lanes read LP-11.
- `cv1800_vi` parameters: `irq_log=N` logs the first N interrupts of each
  stream; `skip_frames` (default 1) is described under Limitations.
- `cv1800_csi2` parameters: `hs_settle` (0, the default, derives the D-PHY
  settle time from the sensor's link frequency), `mac_mhz`.

Never read the VI or CSI-2 registers through `/dev/mem`: while the blocks are
not clocked, the access hangs the bus and the board with it. The debugfs files
only read while the hardware is powered.

## Limitations

- Only the Camera Module 3 and v1 on J2 and the CAM-GC2083 on J1 are
  described. Other sensors need their own overlay, and a driver where
  mainline has none.
- The ISP path is a first version, see "The ISP path".
- The first frame after the sensor starts is short. The bridge drops it, but a
  front end armed for it never finishes, so `cv1800-vi` arms only after
  `skip_frames` frames have gone by.
- Restarting the IMX708 from software standby stalls it (it reports streaming
  but its frame counter stops), so the driver powers it off as soon as it
  stops streaming and every start programs it from scratch.
- OV5647 at 2592x1944: the sensor sends extra packets that the receiver
  counts as CRC errors. Image lines stay intact. Once per boot, one of these
  packets adds a line to a frame. The bridge then drops every frame until
  `cv1800-vi` resets it, which costs a few frames.
- OV5647: the colour-bar test pattern comes out in BGGR order, while live
  images are GBRG as the driver reports. Only the test pattern is affected.
- The bridge's colour bars move from frame to frame at some widths, for
  example 2752 and 2880, on both nodes. Use a ramp pattern at those widths
  (`PATTERN=2` for `tools/stream-yuv.sh`).

## The ISP path

The front end writes each raw frame to an internal buffer. The ISP's back end
reads it back and runs the raw, RGB and YUV stages, which write the frame to
the capture buffer as 4:2:0 YUV. The vendor stack calls this the offline back
end mode. The register sequence follows the vendor's GPL kernel driver
(`osdrv/interdrv/vi` in Milk-V's duo-buildroot-sdk-v2).

What runs: black level, white balance gains, demosaic, the sRGB gamma curve,
and the RGB to YUV matrix at its reset value. What does not: lens shading,
the colour matrix, defect pixel correction, noise reduction, sharpening,
tone mapping, and all statistics. Nothing sets the gains automatically yet:
an application measures the image and sets the controls.

Not known yet, to check on hardware:

- Whether the output is NV21 as the vendor stack uses it, or NV12. The
  colour bars show it: with the wrong order, red and blue swap.
- The scale and range of the matrix at reset. The status file shows it.
  The driver reports limited-range BT.601 until that is checked.
- Sizes other than 1920x1080. Frames 2896 or more wide stall on every
  frame, so the driver limits the width to 2880. The ISP's height fields
  stop at 4096.

## Layout

    linux/        the drivers, overlays and bindings at their kernel paths,
                  with the Kconfig and Makefiles that build them in-tree
    armbian/      the overlay Makefile lines Armbian's extension appends
    src/          DKMS only: dw9817-vcm and the CCI wrappers
    src/linux/    DKMS only: private copies of kernel code: the CCS PLL
                  calculator with the patch it needs, the CCI helpers, and
                  mainline's ov5647 driver (scripts/update-linux-copies.sh)
    packaging/    Debian package metadata and maintainer scripts
    scripts/      package and copy-refresh scripts
    tools/        capture helpers: raw (stream.sh) and YUV (stream-yuv.sh)
    extras/       the libcamera change

## License

GPL-2.0. The overlays are GPL-2.0 OR MIT. `packaging/copyright` lists the code
taken from Linux and from the IMX708 series, with its authors.
