/*
 * ataidentify.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2012 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ATAIDENTIFY_H
#define ATAIDENTIFY_H

#define ATAIDENTIFY_H_CVSID "$Id$"

void ata_print_identify_data(const void * id, bool all_words, int bit_level);

#endif // ATAIDENTIFY_H
