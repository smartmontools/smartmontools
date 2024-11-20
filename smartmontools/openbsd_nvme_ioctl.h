/*
 * Copyright (c) 2024 Kenneth R Westerback <krw@openbsd.org>
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

#define	NVME_PASSTHROUGH_CMD		_IOWR('n', 0, struct nvme_pt_cmd)

struct nvme_pt_status {
	int			ps_dv_unit;
	int			ps_nsid;
	int			ps_flags;
#define NVME_CQE_SCT(_f)	((_f) & (0x07 << 9))
#define  NVME_CQE_SCT_GENERIC		(0x00 << 9)
#define NVME_CQE_SC(_f)		((_f) & (0xff << 1))
#define  NVME_CQE_SC_SUCCESS		(0x00 << 1)
	uint32_t		ps_csts;
	uint32_t		ps_cc;
};

#define	BIO_MSG_COUNT	5
#define	BIO_MSG_LEN	128

struct bio_msg {
	int		bm_type;
	char		bm_msg[BIO_MSG_LEN];
};

struct bio_status {
	char		bs_controller[16];
	int		bs_status;
	int		bs_msg_count;
	struct bio_msg	bs_msgs[BIO_MSG_COUNT];
};

struct bio {
	void			*bio_cookie;
	struct bio_status	bio_status;
};

struct nvme_pt_cmd {
	/* Commands may arrive via /dev/bio. */
	struct bio		pt_bio;

	/* The sqe fields that the caller may specify. */
	uint8_t			pt_opcode;
	uint32_t		pt_nsid;
	uint32_t		pt_cdw10;
	uint32_t		pt_cdw11;
	uint32_t		pt_cdw12;
	uint32_t		pt_cdw13;
	uint32_t		pt_cdw14;
	uint32_t		pt_cdw15;

	caddr_t			pt_status;
	uint32_t		pt_statuslen;

	caddr_t			pt_databuf;	/* User space address. */
	uint32_t		pt_databuflen;	/* Length of buffer. */
};

#define	nvme_completion_is_error(_flags)				\
	((NVME_CQE_SC(_flags) != NVME_CQE_SC_SUCCESS)   		\
	    || (NVME_CQE_SCT(_flags) != NVME_CQE_SCT_GENERIC))

