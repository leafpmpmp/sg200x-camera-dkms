# sg200x-camera-dkms

Camera support for Sophgo SG200x boards on mainline Linux. Armbian builds it
into its sophgo-sg200x kernels; other kernels build it on the board with DKMS.

Tested on the Milk-V Duo S under Armbian with Linux 7.2, on both of its cores
(C906 riscv64 and Cortex-A53 arm64): a Raspberry Pi Camera Module 3 and a
Camera Module v1 on J2, and a Milk-V CAM-GC2083 on J1 (riscv64 only).

## What is in it

| Module        | What it does |
|---------------|--------------|
| `cv1800-vi`   | Raw capture through the VI block's CSI bridge: frames go to memory as packed 12-bit Bayer, with no ISP processing. Media-controller driver with a bridge sub-device (including the bridge's test pattern generator) and a video node. |
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
  ones. Armbian's sophgo-sg200x kernels reserve 128 MiB.
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
  size.
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
- Raw capture only: the SoC's hardware ISP is not used.
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
    tools/        capture helper
    extras/       the libcamera change

## License

GPL-2.0. The overlays are GPL-2.0 OR MIT. `packaging/copyright` lists the code
taken from Linux and from the IMX708 series, with its authors.
