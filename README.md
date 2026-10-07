# sg200x-camera-dkms

Camera support for Sophgo SG200x boards on mainline Linux, kept out of the
kernel tree and built on the board by DKMS.

Tested on the Milk-V Duo S with a Raspberry Pi Camera Module 3, on both of its
cores (C906 riscv64 and Cortex-A53 arm64), under Armbian with Linux 7.2.

## What is in it

| Module        | What it does |
|---------------|--------------|
| `cv1800-vi`   | Raw capture through the VI block's CSI bridge: frames go to memory as packed 12-bit Bayer, with no ISP processing. Media-controller driver with a bridge sub-device (including the bridge's test pattern generator) and a video node. |
| `cv1800-csi2` | MIPI CSI-2 receiver: MAC 0 and the D-PHY, lanes taken from the device tree. |
| `imx708`      | Sony IMX708 driver from its upstream v2 series, plus the 2x2 binned 2304x1296 mode and power-off between streams (see Limitations). |
| `dw9817-vcm`  | The Camera Module 3's focus motor driver, based on `dw9807-vcm`. |

Plus the device tree overlay `sg2000-camera-j2-imx708`, for a Camera Module 3
on the Duo S J2 connector.

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
    apt install ./sg200x-camera-dkms_0.1.0_riscv64.deb

Enable the overlay by adding it to `user_overlays=` in `/boot/armbianEnv.txt`,
then reboot:

    user_overlays=sg2000-camera-j2-imx708

`media-ctl -p` should then show the sensor, the CSI-2 receiver, the bridge
and the video node.

Without the package, `make KDIR=/path/to/kernel/build` builds the modules, and
`dkms add .` registers the tree with DKMS.

## Using it

- **Raw frames**: `tools/stream.sh [width height] [pattern] [frames]` sets up
  the pipeline and captures to `/root/cap.raw`. Pattern 0 is the live image,
  1 the sensor's colour bars. Frames are packed RAW12 (the MIPI layout, lines
  padded to 16 bytes) with the sensor's 10 bits in the top bits.
- **libcamera**: its simple pipeline handler and software ISP turn the camera
  into an ordinary RGB camera for `cam`, GStreamer's `libcamerasrc`, PipeWire
  and V4L2 applications under `libcamerify`. libcamera has to know the
  `cv1800-vi` driver: `extras/libcamera/libcamera-sg200x.patch` (against
  v0.7.2) adds it, with an IMX708 tuning file. The software ISP runs at about
  15 fps at 720p on the Cortex-A53 and 5 fps on the C906.
- **Focus**: `focus_absolute` (0-1023) on the `dw9817` sub-device. The lens is
  only powered while something holds that node open; libcamera does.
- **Orientation**: the overlay declares the sensor as rotated by 180 degrees,
  so libcamera flips it upright.

The sensor itself delivers 56 fps binned and 14.35 fps at full resolution.

## Debugging

- `/sys/kernel/debug/a000000.video-input/status`: what the CSI bridge
  measured in the incoming frames, and every status bit seen since the stream
  started.
- `/sys/kernel/debug/a0c2000.csi/status`: receiver state, decoded data type,
  lane LP states and error counts. Writing 1 to `power` powers the receiver
  without a stream, to check that an idle sensor's lanes read LP-11.
- `cv1800_vi` parameters: `irq_log=N` logs the first N interrupts of each
  stream; `skip_frames` (default 1) is described under Limitations.
- `cv1800_csi2` parameters: `hs_settle`, `mac_mhz`.

Never read the VI or CSI-2 registers through `/dev/mem`: while the blocks are
not clocked, the access hangs the bus and the board with it. The debugfs files
only read while the hardware is powered.

## Limitations

- Only the Camera Module 3 on J2 is described. Other sensors need their own
  overlay, and a driver where mainline has none. J1, Milk-V's camera
  connector, uses other D-PHY lanes and is untested.
- Raw capture only: the SoC's hardware ISP is not used.
- The first frame after the sensor starts is short. The bridge drops it, but a
  front end armed for it never finishes, so `cv1800-vi` arms only after
  `skip_frames` frames have gone by.
- Restarting the IMX708 from software standby stalls it (it reports streaming
  but its frame counter stops), so the driver powers it off as soon as it
  stops streaming and every start is a cold start.

## Layout

    src/          the drivers
    src/linux/    private copies of kernel code the imx708 module needs, and
                  the patch applied to them (scripts/update-linux-copies.sh)
    dts/          the overlay
    packaging/    Debian package metadata and maintainer scripts
    scripts/      package and copy-refresh scripts
    tools/        capture helper
    extras/       the libcamera change

## License

GPL-2.0. The overlay is GPL-2.0 OR MIT. `packaging/copyright` lists the code
taken from Linux and from the IMX708 series, with its authors.
