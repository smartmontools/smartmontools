/*	$NetBSD: nvmereg.h,v 1.1 2016/05/01 10:21:02 nonaka Exp $	*/
/*	$OpenBSD: nvmereg.h,v 1.10 2016/04/14 11:18:32 dlg Exp $ */

/*
 * Copyright (c) 2014 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>

struct nvme_sge {
	uint8_t		id;
	uint8_t		_reserved[15];
} __packed __aligned(8);

struct nvme_sqe {
	uint8_t		opcode;
	uint8_t		flags;
	uint16_t	cid;

	uint32_t	nsid;

	uint8_t		_reserved[8];

	uint64_t	mptr;

	union {
		uint64_t	prp[2];
		struct nvme_sge	sge;
	} __packed	entry;

	uint32_t	cdw10;
	uint32_t	cdw11;
	uint32_t	cdw12;
	uint32_t	cdw13;
	uint32_t	cdw14;
	uint32_t	cdw15;
} __packed __aligned(8);

struct nvme_cqe {
	uint32_t	cdw0;

	uint32_t	_reserved;

	uint16_t	sqhd; /* SQ Head Pointer */
	uint16_t	sqid; /* SQ Identifier */

	uint16_t	cid; /* Command Identifier */
	uint16_t	flags;
#define NVME_CQE_DNR		__BIT(15)
#define NVME_CQE_M		__BIT(14)
#define NVME_CQE_SCT(_f)	((_f) & (0x07 << 8))
#define  NVME_CQE_SCT_GENERIC		(0x00 << 8)
#define  NVME_CQE_SCT_COMMAND		(0x01 << 8)
#define  NVME_CQE_SCT_MEDIAERR		(0x02 << 8)
#define  NVME_CQE_SCT_VENDOR		(0x07 << 8)
#define NVME_CQE_SC(_f)		((_f) & (0x7f << 1))
#define  NVME_CQE_SC_SUCCESS		(0x00 << 1)
#define  NVME_CQE_SC_INVALID_OPCODE	(0x01 << 1)
#define  NVME_CQE_SC_INVALID_FIELD	(0x02 << 1)
#define  NVME_CQE_SC_CID_CONFLICT	(0x03 << 1)
#define  NVME_CQE_SC_DATA_XFER_ERR	(0x04 << 1)
#define  NVME_CQE_SC_ABRT_BY_NO_PWR	(0x05 << 1)
#define  NVME_CQE_SC_INTERNAL_DEV_ERR	(0x06 << 1)
#define  NVME_CQE_SC_CMD_ABRT_REQD	(0x07 << 1)
#define  NVME_CQE_SC_CMD_ABDR_SQ_DEL	(0x08 << 1)
#define  NVME_CQE_SC_CMD_ABDR_FUSE_ERR	(0x09 << 1)
#define  NVME_CQE_SC_CMD_ABDR_FUSE_MISS	(0x0a << 1)
#define  NVME_CQE_SC_INVALID_NS		(0x0b << 1)
#define  NVME_CQE_SC_CMD_SEQ_ERR	(0x0c << 1)
#define  NVME_CQE_SC_INVALID_LAST_SGL	(0x0d << 1)
#define  NVME_CQE_SC_INVALID_NUM_SGL	(0x0e << 1)
#define  NVME_CQE_SC_DATA_SGL_LEN	(0x0f << 1)
#define  NVME_CQE_SC_MDATA_SGL_LEN	(0x10 << 1)
#define  NVME_CQE_SC_SGL_TYPE_INVALID	(0x11 << 1)
#define  NVME_CQE_SC_LBA_RANGE		(0x80 << 1)
#define  NVME_CQE_SC_CAP_EXCEEDED	(0x81 << 1)
#define  NVME_CQE_NS_NOT_RDY		(0x82 << 1)
#define  NVME_CQE_RSV_CONFLICT		(0x83 << 1)
#define NVME_CQE_PHASE		__BIT(0)
} __packed __aligned(8);

/*-
 * Copyright (C) 2012-2013 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define	NVME_PASSTHROUGH_CMD		_IOWR('n', 0, struct nvme_pt_command)

#define	nvme_completion_is_error(cpl)					\
	((NVME_CQE_SC((cpl)->flags) != NVME_CQE_SC_SUCCESS)   		\
	    || (NVME_CQE_SCT((cpl)->flags) != NVME_CQE_SCT_GENERIC))

struct nvme_pt_command {

	/*
	 * cmd is used to specify a passthrough command to a controller or
	 *  namespace.
	 *
	 * The following fields from cmd may be specified by the caller:
	 *	* opcode
	 *	* nsid (namespace id) - for admin commands only
	 *	* cdw10-cdw15
	 *
	 * Remaining fields must be set to 0 by the caller.
	 */
	struct nvme_sqe		cmd;

	/*
	 * cpl returns completion status for the passthrough command
	 *  specified by cmd.
	 *
	 * The following fields will be filled out by the driver, for
	 *  consumption by the caller:
	 *	* cdw0
	 *	* flags (except for phase)
	 *
	 * Remaining fields will be set to 0 by the driver.
	 */
	struct nvme_cqe		cpl;

	/* buf is the data buffer associated with this passthrough command. */
	void			*buf;

	/*
	 * len is the length of the data buffer associated with this
	 *  passthrough command.
	 */
	uint32_t		len;

	/*
	 * is_read = 1 if the passthrough command will read data into the
	 *  supplied buffer from the controller.
	 *
	 * is_read = 0 if the passthrough command will write data from the
	 *  supplied buffer to the controller.
	 */
	uint32_t		is_read;

	/*
	 * timeout (unit: ms)
	 *
	 * 0: use default timeout value
	 */
	uint32_t		timeout;
};

#define NVME_PREFIX		"/dev/nvme"
#define NVME_NS_PREFIX		"ns"
