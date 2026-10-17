# SPDX-License-Identifier: GPL-2.0
#
# DKMS build. The drivers live under linux/ at their kernel paths: Armbian's
# sophgo-sg200x-camera extension copies them into the kernel tree instead.
obj-m := cv1800-vi.o cv1800-csi2.o dw9817-vcm.o gc2083.o imx708.o ov5647.o

cv1800-vi-y := linux/drivers/media/platform/sophgo/cv1800-vi.o
cv1800-csi2-y := linux/drivers/media/platform/sophgo/cv1800-csi2.o
dw9817-vcm-y := src/dw9817-vcm.o

# Neither sensor is in mainline yet. Each module carries a private copy of the
# V4L2 CCI register helpers, and the IMX708 one also of the CCS PLL calculator
# with the change the driver needs, so that DKMS needs no kernel patch and no
# kernel option beyond the camera sensor framework. See src/linux/.
sensors := linux/drivers/media/i2c/sg200x-camera
imx708-y := $(sensors)/imx708.o src/linux/ccs-pll.o src/linux/v4l2-cci.o
CFLAGS_$(sensors)/imx708.o := -I$(src)/src/linux -DCONFIG_V4L2_CCI_I2C_MODULE=1
CFLAGS_src/linux/v4l2-cci.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1
gc2083-y := $(sensors)/gc2083.o src/gc2083-cci.o
CFLAGS_$(sensors)/gc2083.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1
CFLAGS_src/gc2083-cci.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1

# Mainline's OV5647 driver (Raspberry Pi Camera Module v1), which Armbian
# kernels do not build, with its own copy of the CCI helpers.
ov5647-y := src/linux/ov5647.o src/ov5647-cci.o
CFLAGS_src/linux/ov5647.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1
CFLAGS_src/ov5647-cci.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1
