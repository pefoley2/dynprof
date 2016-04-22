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

#include "libdynprof.h"

void __dynprof_register_handler() {
    if (atexit(exit_handler)) {
        std::cerr << "Failed to register atexit handler." << std::endl;
        exit(1);
    }
}
void exit_handler() {
    // FIXME: re-work.
    std::cerr << "Summary:" << std::endl;
    std::cerr << "%\tcumulative\tself\t\t\t\tself\ttotal" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\t\tcalls\tTs/call\tTs/call\tname" << std::endl;
}
