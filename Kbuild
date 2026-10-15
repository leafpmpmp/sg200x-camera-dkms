# SPDX-License-Identifier: GPL-2.0
obj-m := cv1800-vi.o cv1800-csi2.o dw9817-vcm.o imx708.o ov5647.o

cv1800-vi-y := src/cv1800-vi.o
cv1800-csi2-y := src/cv1800-csi2.o
dw9817-vcm-y := src/dw9817-vcm.o

# The IMX708 driver is not in mainline yet. It carries private copies of the
# CCS PLL calculator (with the change the driver needs) and of the V4L2 CCI
# register helpers, so that it needs no kernel patch and no kernel option
# beyond the camera sensor framework. See src/linux/.
imx708-y := src/imx708.o src/linux/ccs-pll.o src/linux/v4l2-cci.o
CFLAGS_src/imx708.o := -I$(src)/src/linux -DCONFIG_V4L2_CCI_I2C_MODULE=1
CFLAGS_src/linux/v4l2-cci.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1

# Mainline's OV5647 driver (Raspberry Pi Camera Module v1), which Armbian
# kernels do not build, with its own copy of the CCI helpers.
ov5647-y := src/linux/ov5647.o src/ov5647-cci.o
CFLAGS_src/linux/ov5647.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1
CFLAGS_src/ov5647-cci.o := -DCONFIG_V4L2_CCI_I2C_MODULE=1
