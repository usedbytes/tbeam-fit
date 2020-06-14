// SPDX-License-Identifier: MIT
// Copyright (c) 2020 Brian Starkey <stark3y@gmail.com>
#ifndef __FIT_H__
#define __FIT_H__

#include "gen/fit.h"
#include "gen/fit_product.h"

struct fit_file;

struct fit_file *fit_create(const char *filename,
			    FIT_DATE_TIME timestamp,
			    FIT_FILE type);

int fit_finalise(struct fit_file *fit);
int fit_destroy(struct fit_file *fit);

int fit_register_message(struct fit_file *fit, FIT_MESG_NUM local_id, FIT_MESG mesg);
int fit_write_message(struct fit_file *fit, FIT_MESG_NUM local_id, void *mesg);

int fit_init_message(FIT_MESG type, void *mesg);

#endif /* __FIT_H__ */
