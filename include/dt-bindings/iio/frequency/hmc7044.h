// SPDX-License-Identifier: GPL-2.0
/*
 * HMC7044 SPI Low Jitter Clock Generator
 *
 * Copyright 2019 Analog Devices Inc.
 */

#ifndef _DT_BINDINGS_IIO_FREQUENCY_HMC7044_H_
#define _DT_BINDINGS_IIO_FREQUENCY_HMC7044_H_

/*
 * adi,pulse-generator-mode
 */
#define HMC7044_PULSE_GEN_LEVEL_SENSITIVE	0
#define HMC7044_PULSE_GEN_1_PULSE		1
#define HMC7044_PULSE_GEN_2_PULSE		2
#define HMC7044_PULSE_GEN_4_PULSE		3
#define HMC7044_PULSE_GEN_8_PULSE		4
#define HMC7044_PULSE_GEN_16_PULSE		5
#define HMC7044_PULSE_GEN_CONT_PULSE		7

/*
 * adi,driver-mode
 */
#define HMC7044_DRIVER_MODE_CML			0
#define HMC7044_DRIVER_MODE_LVPECL		1
#define HMC7044_DRIVER_MODE_LVDS		2
#define HMC7044_DRIVER_MODE_CMOS		3

/*
 * adi,driver-impedance
 */
#define HMC7044_DRIVER_IMPEDANCE_DISABLE	0
#define HMC7044_DRIVER_IMPEDANCE_100_OHM	1
#define HMC7044_DRIVER_IMPEDANCE_50_OHM		3

/*
 * adi,output-mux-mode
 */

#define HMC7044_OUTPUT_MUX_CH_DIV		0
#define HMC7044_OUTPUT_MUX_ANALOG_DELAY		1
#define HMC7044_OUTPUT_MUX_GROUP_PAIR		3
#define HMC7044_OUTPUT_MUX_VCO_CLOCK		4

#endif /* _DT_BINDINGS_IIO_FREQUENCY_HMC7044_H_ */
