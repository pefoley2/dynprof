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

#include <chrono>
#include <cstdlib>
#include <iostream>

class FuncOutput {
   public:
    FuncOutput() : before(0), after(0), name(), count(0) {}
    FuncOutput(const FuncOutput&) = delete;
    FuncOutput& operator=(const FuncOutput&) = delete;
    double before;
    double after;
    std::string name;
    int count;
    int padding;  // FIXME
    friend std::ostream& operator<<(std::ostream& os, const FuncOutput& func);
};

void __dynprof_register_handler(int len) __attribute__((visibility("default")));
// double elapsed_time(struct timespec* before, struct timespec* after)
// __attribute__((visibility("default")));
void exit_handler(void);

#endif
