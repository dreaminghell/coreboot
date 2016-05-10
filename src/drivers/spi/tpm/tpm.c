/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This is a driver for a SPI interfaced TPM2 device.
 *
 * It assumes that the required SPI interface has been initialized before the
 * driver is started. A 'sruct spi_slave' pointer passed at initialization is
 * used to direct traffic to the correct SPI interface. This dirver does not
 * provide a way to instantiate multiple TPM devices. Also, to keep things
 * simple, the driver assumes use of TPM locality zero.
 *
 * References to documentation use the TCG issued "TPM Profile (PTP)
 * Specification Revision 00.43" which can be found at http://goo.gl/nKp2lU .
 */
#include <console/console.h>
#include <delay.h>
#include <endian.h>
#include <string.h>
#include <timer.h>

#include "tpm.h"

/* SPI Interface descriptor used by the driver. */
struct tpm_spi_if {
	struct spi_slave *slave;
	int (*cs_assert)(struct spi_slave *slave);
	void (*cs_deassert)(struct spi_slave *slave);
	int  (*xfer)(struct spi_slave *slave, const void *dout,
		     unsigned bytesout, void *din,
		     unsigned bytesin);
};

/* Use the common SPI driver wrapper as the interface callbacks. */
static struct tpm_spi_if tpm_if = {
	.cs_assert = spi_claim_bus,
	.cs_deassert = spi_release_bus,
	.xfer = spi_xfer
};

/* Cached TPM device identification. */
struct tpm2_info tpm_info;

/* TODO(vbendeb): make CONFIG_DEBUG_TPM an int. */
const static int debug_level_ = CONFIG_DEBUG_TPM;

/* Locality management bits (in TPM_ACCESS_REG) */
enum tpm_access_bits {
	tpm_reg_valid_sts = (1 << 7),
	active_locality = (1 << 5),
	request_use = (1 << 1),
	tpm_establishment = (1 << 0),
};

/*
 * Variuous fields of the TPM status register, arguably the most important
 * register when interfacing to a TPM.
 */
enum tpm_sts_bits {
	tpm_family_shift = 26,
	tpm_family_mask = ((1 << 2) - 1),  /* 2 bits wide. */
	tpm_family_tpm2 = 1,
	reset_establishment_bit = (1 << 25),
	command_cancel = (1 << 24),
	burst_count_shift = 8,
	burst_count_mask = ((1 << 16) -1),  /* 16 bits wide. */
	sts_valid = (1 << 7),
	command_ready = (1 << 6),
	tpm_go = (1 << 5),
	data_avail = (1 << 4),
	expect = (1 << 3),
	self_test_done = (1 << 2),
	response_retry = (1 << 1),
};

/*
 * SPI frame header for TPM transactions is 4 bytes in size, it is described
 * in section "6.4.6 Spi Bit Protocol".
 */
typedef struct {
	unsigned char body[4];
} spi_frame_header;

void tpm2_get_info(struct tpm2_info *info)
{
	*info = tpm_info;
}

/*
 * Each TPM2 SPI transaction starts the same: CS is asserted, the 4 byte
 * header is sent to the TPM, the master verifies that the TPM is ready to
 * continue.
 */
static void start_transaction(int read_write, size_t bytes, unsigned addr)
{
	spi_frame_header header;
	uint8_t byte;
	int i;

	/*
	 * Give it 10 ms. TODO(vbendeb): remove this once cr50 SPS TPM driver
	 * performance is fixed.
	 */
	udelay(10000);

	/*
	 * The first byte of the frame header encodes the transaction type
	 * (read or write) and transfer size (set to lentgh - 1), limited to
	 * 64 bytes.
	 */
	header.body[0] = (read_write ? 0x80 : 0) | 0x40 | (bytes - 1);

	/* The rest of the frame header is the TPM register address. */
	for (i = 0; i < 3; i++)
		header.body[i + 1] = (addr >> (8 * (2 - i))) & 0xff;

	/*
	 * The TCG TPM over SPI specification introduces the notion of SPI
	 * flow control (Section "6.4.5 Flow Control").
	 *
	 * Again, the slave (TPM device) expects each transaction to start
	 * with a 4 byte header trasmitted by master. The header indicates if
	 * the master needs to read or write a register, and the register
	 * address.
	 *
	 * If the slave needs to stall the transaction (for instance it is not
	 * ready to send the register value to the master), it sets the MOSI
	 * line to 0 during the last clock of the 4 byte header. In this case
	 * the master is supposed to start polling the SPI bus, byte at time,
	 * until the last bit in the received byte (transferred during the
	 * last clock of the byte) is set to 1.
	 */
	tpm_if.cs_assert(tpm_if.slave);

	/*
	 * Due to some SPI controllers shortcomings (Rockchip comes to
	 * mind...) we trasmit the 4 byte header without checking the byte
	 * transmitted by the TPM during the transaction's last byte.
	 *
	 * We know that Haven is guaranteed to set the flow control bit to 0
	 * during the header transfer, but real TPM2 might be fast enough not
	 * to require to stall the master, this would present an issue.
	 */
	tpm_if.xfer(tpm_if.slave, header.body, sizeof(header.body), NULL, 0);

	/* Now poll the bus until TPM removes the stall bit. */
	do {
		tpm_if.xfer(tpm_if.slave, NULL, 0, &byte, 1);
	} while (!(byte & 1));
}

/*
 * Print out the contents of a buffer, if debug is enabled. Skip registers
 * other than FIFO, unless debug_level_ is 2.
 */
static void trace_dump(const char *prefix, uint32_t reg,
		       size_t bytes, const uint8_t *buffer,
		       int force)
{
	static char prev_prefix;
	static unsigned prev_reg;
	static int current_char;

#define BYTES_PER_LINE 32

	if (!force) {
		if (!debug_level_)
			return;

		if ((debug_level_ < 2) && (reg != 0x24))
			return;
	}

	if ((prev_prefix != *prefix) || (prev_reg != reg)) {
		prev_prefix = *prefix;
		prev_reg = reg;
		printk(BIOS_DEBUG, "\n%s %2.2x:", prefix, reg);
		current_char = 0;
	}

	if ((reg != 0x24) && (bytes == 4)) {
		printk(BIOS_DEBUG, " %8.8x", *(const uint32_t*) buffer);
	} else {
		int i;
		for (i = 0; i < bytes; i++) {
			if (current_char && !(current_char % BYTES_PER_LINE)) {
				printk(BIOS_DEBUG, "\n     ");
				current_char = 0;
			}
			current_char++;
			printk(BIOS_DEBUG, " %2.2x", buffer[i]);
		}
	}
}

/*
 * Once transaction is initiated and the TPM indicated that it is ready to go,
 * read the actual bytes from the register.
 */
static void write_bytes(const void *buffer, size_t bytes)
{
	tpm_if.xfer(tpm_if.slave, buffer, bytes, NULL, 0);
}

/*
 * Once transaction is initiated and the TPM indicated that it is ready to go,
 * write the actual bytes to the register.
 */
static void read_bytes(void *buffer, size_t bytes)
{
	tpm_if.xfer(tpm_if.slave, NULL, 0, buffer, bytes);
}

/*
 * To write a register, start transaction, transfer data to the TPM, deassert
 * CS when done.
 */
static int tpm2_write_reg(unsigned reg_number, size_t bytes, const void *buffer)
{
	trace_dump("W", reg_number, bytes, buffer, 0);
	start_transaction(false, bytes, reg_number);
	write_bytes(buffer, bytes);
	tpm_if.cs_deassert(tpm_if.slave);
	return true;
}

/*
 * To read a register, start transaction, transfer data from the TPM, deassert
 * CS when done.
 */
static int tpm2_read_reg(unsigned reg_number, size_t bytes, void *buffer)
{
	start_transaction(true, bytes, reg_number);
	read_bytes(buffer, bytes);
	tpm_if.cs_deassert(tpm_if.slave);
	trace_dump("R", reg_number, bytes, buffer, 0);
	return true;
}

/*
 * Status register is accessed often, wrap reading and writing it into
 * dedicated functions.
 */
static int read_tpm_sts(uint32_t *status)
{
	return tpm2_read_reg(TPM_STS_REG, sizeof(*status), status);
}

static int write_tpm_sts(uint32_t status)
{
	return tpm2_write_reg(TPM_STS_REG, sizeof(status), &status);
}

/*
 * The TPM may limit the transaction bytes count (burst count) below the 64
 * bytes max. The current value is available as a field of the status
 * register.
 */
static uint32_t get_burst_count(void)
{
	uint32_t status;

	read_tpm_sts(&status);
	return (status >> burst_count_shift) & burst_count_mask;
}

int tpm2_init(struct spi_slave *spi_if) {
	uint32_t did_vid, status;
	uint8_t cmd;

	tpm_if.slave = spi_if;

	tpm2_read_reg(TPM_DID_VID_REG, sizeof(did_vid), &did_vid);

	/* Try claiming locality zero. */
	tpm2_read_reg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
	if ((cmd & (active_locality & tpm_reg_valid_sts)) ==
	    (active_locality & tpm_reg_valid_sts)) {
		/*
		 * Locality active - maybe reset line is not connected?
		 * Release the locality and try again
		 */
		cmd = active_locality;
		tpm2_write_reg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
		tpm2_read_reg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
	}

	/* The tpm_establishment bit can be either set or not, ignore it. */
	if ((cmd & ~tpm_establishment) != tpm_reg_valid_sts) {
		printk(BIOS_ERR, "invalid reset status: %#x\n", cmd);
		return -1;
	}

	cmd = request_use;
	tpm2_write_reg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
	tpm2_read_reg(TPM_ACCESS_REG, sizeof(cmd), &cmd);
	if ((cmd &  ~tpm_establishment) !=
	    (tpm_reg_valid_sts | active_locality)) {
		printk(BIOS_ERR, "failed to claim locality 0, status: %#x\n",
		       cmd);
		return -1;
	}

	read_tpm_sts(&status);
	if (((status >> tpm_family_shift) & tpm_family_mask) !=
	    tpm_family_tpm2) {
		printk(BIOS_ERR, "unexpected TPM family value, status: %#x\n",
		       status);
		return -1;
	}

	/*
	 * Locality claimed, read the revision value and set up the tpm_info
	 * structure.
	 */
	tpm2_read_reg(TPM_RID_REG, sizeof(cmd), &cmd);
	tpm_info.vendor_id = did_vid & 0xffff;
	tpm_info.device_id = did_vid >> 16;
	tpm_info.revision = cmd;

	printk(BIOS_INFO, "Connected to device vid:did:rid of %4.4x:%4.4x:%2.2x\n",
	       tpm_info.vendor_id, tpm_info.device_id, tpm_info.revision);

	return 0;
}

/*
 * This is in seconds, certain TPM commands, like key generation, can take
 * long time to complete.
 */
#define MAX_STATUS_TIMEOUT 120
static int wait_for_status(uint32_t status_mask, uint32_t status_expected)
{
  uint32_t status;
  struct stopwatch sw;

  stopwatch_init_usecs_expire(&sw, MAX_STATUS_TIMEOUT * 1000 * 1000);
  do {
	  udelay(10000);
	  if (stopwatch_expired(&sw)) {
		  printk(BIOS_ERR, "failed to get expected status %x\n",
			 status_expected);
		  return false;
	  }
	  read_tpm_sts(&status);
  } while ((status & status_mask) != status_expected);

  return true;
}

/*
 * Transfer requested number of bytes to or from TPM FIFO, accounting for the
 * current burst count value.
 */
static void bulk_transfer(size_t transfer_size,
			  uint8_t *buffer, int read_direction)
{
	uint32_t transaction_size;
	uint32_t burst_count;
	size_t handled_so_far = 0;

	do {
		do {
			/* Could be zero when TPM is busy. */
			burst_count = get_burst_count();
		} while (!burst_count);

		transaction_size = transfer_size - handled_so_far;
		if (transaction_size > burst_count)
			transaction_size = burst_count;

		/*
		 * The SPI frame header does not allow to pass more than 64
		 * bytes.
		 */
		if (transaction_size > 64)
			transaction_size = 64;

		if (read_direction)
			tpm2_read_reg(TPM_DATA_FIFO_REG, transaction_size,
				      buffer + handled_so_far);
		else
			tpm2_write_reg(TPM_DATA_FIFO_REG, transaction_size,
				       buffer + handled_so_far);
		handled_so_far += transaction_size;

	} while(handled_so_far != transfer_size);
}

size_t tpm2_process_command(const void *tpm2_command, size_t command_size,
			    void *tpm2_response, size_t max_response)
{
	uint32_t status;
	uint32_t expected_status_bits;
	size_t payload_size;
	size_t bytes_to_go;
	const uint8_t *cmd_body = tpm2_command;
	uint8_t *rsp_body = tpm2_response;

	memcpy(&payload_size, cmd_body + 2, sizeof(payload_size));
	payload_size = be32toh(payload_size);
	/* Sanity check. */
	if (payload_size != command_size) {
		printk(BIOS_ERR,
		       "Command size mismatch: encoded %zd != requested %zd\n",
		       payload_size, command_size);
		trace_dump("W", TPM_DATA_FIFO_REG, command_size, cmd_body, 1);
		printk(BIOS_DEBUG, "\n");
		return 0;
	}

	/* Let the TPM know that the command is coming. */
	write_tpm_sts(command_ready);

	/*
	 * Tpm commands and responses written to and read from the FIFO
	 * register (0x24) are datagrams of variable size, prepended by a 6
	 * byte header.
	 *
	 * The specification description of the state machine is a bit vague,
	 * but from experience it looks like there is no need to wait for the
	 * sts.expect bit to be set, at least with the 9670 and Haven devices.
	 * Just write the command into FIFO, making sure not to exceed the
	 * burst count or the maximum PDU size, whatever is smaller.
	 */
	bulk_transfer(command_size, (uint8_t *)cmd_body, 0);

	/* Now tell the TPM it can start processing the command. */
	write_tpm_sts(tpm_go);

	/* Now wait for it to report that the response is ready. */
	expected_status_bits = sts_valid | data_avail;
	if (!wait_for_status(expected_status_bits, expected_status_bits)) {
		/*
		 * If timed out, which should never happen, let's at least
		 * print out the offending command.
		 */
		trace_dump("W", TPM_DATA_FIFO_REG, command_size, cmd_body, 1);
		printk(BIOS_DEBUG, "\n");
		return 0;
	}

#define HEADER_SIZE 6
	/*
	 * The response is ready, let's read it. First we read the FIFO
	 * payload header, to see how much data to expect. The header size is
	 * fixed to six bytes, the total payload size is stored in network
	 * order in the last four bytes of the header.
	 */
	tpm2_read_reg(TPM_DATA_FIFO_REG, HEADER_SIZE, rsp_body);

	/* Find out the total payload size. */
	memcpy(&payload_size, rsp_body + 2, sizeof(payload_size));
	payload_size = be32toh(payload_size);

	if (payload_size > max_response) {
		/*
		 * TODO(vbendeb): at least drain the FIFO here or somehow let
		 * the TPM know that the response can be dropped.
		 */
		printk(BIOS_ERR, " tpm response too long (%zd bytes)",
		       payload_size);
		return 0;
	}

	/*
	 * Now let's read all but the last byte in the FIFO to make sure the
	 * status register is showing correct flow control bits: 'more data'
	 * until the last byte and then 'no more data' once the last byte is
	 * read.
	 */
	bytes_to_go = payload_size - 1 - HEADER_SIZE;
	bulk_transfer(bytes_to_go, rsp_body + HEADER_SIZE, 1);

	/* Verify that there is still data to read. */
	read_tpm_sts(&status);
	if ((status & expected_status_bits) != expected_status_bits) {
		printk(BIOS_ERR, "unexpected intermediate status %#x\n",
		       status);
		return 0;
	}

	/* Read the last byte of the PDU. */
	tpm2_read_reg(TPM_DATA_FIFO_REG, 1, rsp_body + payload_size - 1);

	/* Terminate the dump, if enabled. */
	if (debug_level_)
		printk(BIOS_DEBUG, "\n");

	/* Verify that 'data available' is not asseretd any more. */
	read_tpm_sts(&status);
	if ((status & expected_status_bits) != sts_valid) {
		printk(BIOS_ERR, "unexpected final status %#x\n", status);
		return 0;
	}

	/* Move the TPM back to idle state. */
	write_tpm_sts(command_ready);

	return payload_size;
}
