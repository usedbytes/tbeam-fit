// SPDX-License-Identifier: MIT
// Copyright (c) 2020 Brian Starkey <stark3y@gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fit.h"
#include "gen/fit_product.h"
#include "gen/fit_crc.h"

#define MAX_LOCAL_DEFS 8

struct fit_file {
	FILE *fp;
	FIT_FILE_HDR hdr;
	FIT_UINT16 data_crc;
	FIT_UINT8 mesg_size[MAX_LOCAL_DEFS];
};

struct fit_file *fit_create(const char *filename,
			    FIT_DATE_TIME timestamp,
			    FIT_FILE type)
{
	size_t len;
	struct fit_file *fit = calloc(1, sizeof(*fit));
	if (!fit) {
		return NULL;
	}

	fit->hdr.header_size = FIT_FILE_HDR_SIZE;
	fit->hdr.profile_version = FIT_PROFILE_VERSION;
	fit->hdr.protocol_version = FIT_PROTOCOL_VERSION_20;
	memcpy((FIT_UINT8 *)&fit->hdr.data_type, ".FIT", 4);

	fit->fp = fopen(filename, "w+");
	if (!fit->fp) {
		goto fail;
	}

	// Write the header - ideally we'd just skip it though? fallocate() is
	// non-portable
	len = fwrite(&fit->hdr, 1, FIT_FILE_HDR_SIZE, fit->fp);
	if (len < FIT_FILE_HDR_SIZE) {
		goto fail;
	}

	return fit;

fail:
	if (fit->fp) {
		fclose(fit->fp);
	}
	free(fit);
	return NULL;
}

static inline FIT_UINT16 calc_crc(FIT_UINT16 crc, const uint8_t *data, size_t len)
{
	return FitCRC_Update16(crc, data, len);
}

static size_t write_data(struct fit_file *fit, const uint8_t *data, size_t len)
{
	len = fwrite(data, 1, len, fit->fp);
	fit->hdr.data_size += len;
	fit->data_crc = calc_crc(fit->data_crc, data, len);

	return len;
}

int fit_finalise(struct fit_file *fit)
{
	size_t len;
	int ret;

	// First write the data_crc at the end
	len = fwrite(&fit->data_crc, 1, sizeof(fit->data_crc), fit->fp);
	if (len < sizeof(fit->data_crc)) {
		return -1;
	}

	// Then update the header CRC and write it at the start
	fit->hdr.crc = calc_crc(0, (const uint8_t *)&fit->hdr, FIT_STRUCT_OFFSET(crc, FIT_FILE_HDR));
	ret = fseek(fit->fp, 0, SEEK_SET);
	if (ret != 0) {
		return ret;
	}
	len = fwrite(&fit->hdr, 1, fit->hdr.header_size, fit->fp);
	if (len != fit->hdr.header_size) {
		return -1;
	}

	return 0;
}

int fit_destroy(struct fit_file *fit)
{
	int ret = fclose(fit->fp);
	free(fit);

	return ret;
}

int fit_register_message(struct fit_file *fit, FIT_MESG_NUM local_id, FIT_MESG mesg)
{
	// XXX: It would be really nice if this was part of the autogen.
	static const FIT_UINT8 mesg_sizes[FIT_MESGS][2] = {
		{ FIT_PAD_MESG_DEF_SIZE,                0 },
		{ FIT_FILE_ID_MESG_DEF_SIZE,            FIT_FILE_ID_MESG_SIZE },
		{ FIT_ACTIVITY_MESG_DEF_SIZE,           FIT_ACTIVITY_MESG_SIZE },
		{ FIT_SESSION_MESG_DEF_SIZE,            FIT_SESSION_MESG_SIZE },
		{ FIT_LAP_MESG_DEF_SIZE,                FIT_LAP_MESG_SIZE },
		{ FIT_RECORD_MESG_DEF_SIZE,             FIT_RECORD_MESG_SIZE },
		{ FIT_DEVICE_INFO_MESG_DEF_SIZE,        FIT_DEVICE_INFO_MESG_SIZE },
		{ FIT_ACCELEROMETER_DATA_MESG_DEF_SIZE, FIT_ACCELEROMETER_DATA_MESG_SIZE },
	};

	size_t ret;
	FIT_UINT8 def_size, hdr = local_id | FIT_HDR_TYPE_DEF_BIT;

	if (mesg >= FIT_MESGS) {
		return -1;
	}

	if (local_id >= MAX_LOCAL_DEFS) {
		return -1;
	}

	// Store the size for use when writing messages with this def
	fit->mesg_size[local_id] = mesg_sizes[mesg][1];

	// Mark this as a definition message
	ret = write_data(fit, (const uint8_t *)&hdr, FIT_HDR_SIZE);
	if (ret != FIT_HDR_SIZE) {
		return -1;
	}

	// Write the definition
	def_size = mesg_sizes[mesg][0];
	ret = write_data(fit, (const uint8_t *)fit_mesg_defs[mesg], def_size);
	if (ret != def_size) {
		return -1;
	}

	return 0;
}

int fit_write_message(struct fit_file *fit, FIT_MESG_NUM local_id, void *mesg)
{
	size_t ret;

	if (local_id >= MAX_LOCAL_DEFS) {
		return -1;
	}

	ret = write_data(fit, (const uint8_t *)&local_id, FIT_HDR_SIZE);
	if (ret != FIT_HDR_SIZE) {
		return -1;
	}

	ret = write_data(fit, mesg, fit->mesg_size[local_id]);
	if (ret != fit->mesg_size[local_id]) {
		return -1;
	}

	return 0;
}

int fit_init_message(FIT_MESG type, void *mesg)
{
	if (type >= FIT_MESGS) {
		return -1;
	}

	Fit_InitMesg(fit_mesg_defs[type], mesg);

	return 0;
}
