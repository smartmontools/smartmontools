/*
 * ataidentify.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2012 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ATAIDENTIFY_H
#define ATAIDENTIFY_H

#define ATAIDENTIFY_H_CVSID "$Id$"

void ata_print_identify_data(const void * id, bool all_words, int bit_level);

#endif // ATAIDENTIFY_H
