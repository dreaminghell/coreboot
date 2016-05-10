/*
 * Copyright (C) 2011 Infineon Technologies
 *
 * Authors:
 * Peter Huewe <huewe.external@infineon.com>
 *
 * Version: 2.1.1
 *
 * Description:
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org
 *
 * It is based on the Linux kernel driver tpm.c from Leendert van
 * Dorn, Dave Safford, Reiner Sailer, and Kyleen Hall.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __COREBOOT_SRC_DRIVERS_SPI_TPM_TPM_H
#define __COREBOOT_SRC_DRIVERS_SPI_TPM_TPM_H

#include <stddef.h>
#include <spi-generic.h>

struct tpm2_info {
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t revision;
};

/*
 * Initialize a TPM2 deivce: read its id, claim locality of zero, verify that
 * this indeed is a TPM2 device. Use the passed in handle to access the right
 * SPI port.
 *
 * Return 0 on success, non-zero on failure.
 */
int tpm2_init(struct spi_slave *spi_if);


/*
 * Each command processing consists of sending the command to the TPM, by
 * writing it into the FIFO register, then polling the status register until
 * the TPM is ready to respond, then reading the response from the FIFO
 * regitster. The size of the response can be gleaned from the 6 byte header.
 *
 * This function places the response into the tpm2_response buffer and returns
 * the size of the response.
 */
size_t tpm2_process_command(const void *tpm2_command, size_t command_size,
			    void *tpm2_response, size_t max_response);
void tpm2_get_info(struct tpm2_info *info);

/* Assorted TPM2 registers for interface type FIFO. */
#define TPM_ACCESS_REG       0
#define TPM_STS_REG       0x18
#define TPM_DATA_FIFO_REG 0x24
#define TPM_DID_VID_REG  0xf00
#define TPM_RID_REG      0xf04


/* Size of external transmit buffer (used for stack buffer in tpm_sendrecv) */
#define TPM_BUFSIZE 1260

#endif  /* ! __COREBOOT_SRC_DRIVERS_SPI_TPM_TPM_H */
