/*
 * DynProf, an Dyninst-based dynamic profiler.
 * Copyright (C) 2015-16 Peter Foley
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef LIBDYNPROF_H
#define LIBDYNPROF_H

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <frame.h>
#include <walker.h>

#define OUTPUT_VERSION 1
#define EXPORT __attribute((visibility("default")))

EXPORT extern int __dynprof_output_fd;

EXPORT void __dynprof_register_handler();
EXPORT void __dynprof_get_parent(Dyninst::MachRegisterVal sp, Dyninst::MachRegisterVal fp);

#endif
