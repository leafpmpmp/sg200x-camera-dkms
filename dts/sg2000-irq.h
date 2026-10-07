/* SPDX-License-Identifier: (GPL-2.0 OR MIT) */
/*
 * Peripheral interrupts, numbered the way the shared SG2000 device tree
 * numbers them: on the C906 they go to the PLIC 16 lines up, on the
 * Cortex-A53 they are GIC SPIs. Build arm64 overlays with -DSG2000_ARM64.
 */
#ifndef SG2000_IRQ_H
#define SG2000_IRQ_H

#ifdef SG2000_ARM64
#include <dt-bindings/interrupt-controller/arm-gic.h>
#define SOC_PERIPHERAL_IRQ(nr)	GIC_SPI (nr)
#else
#define SOC_PERIPHERAL_IRQ(nr)	((nr) + 16)
#endif

#endif
