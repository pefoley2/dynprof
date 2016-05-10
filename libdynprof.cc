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

int __dynprof_output_fd;

static void exit_handler() {
    if (close(__dynprof_output_fd) < 0) {
        std::cerr << "Failed to close output file." << std::endl;
        exit(1);
    }
}

void __dynprof_register_handler() {
    if (atexit(exit_handler)) {
        std::cerr << "Failed to register atexit handler." << std::endl;
        exit(1);
    }
    std::string fname = "out_dynprof." + std::to_string(getpid());
    std::string header = "DYNPROF:" + std::to_string(OUTPUT_VERSION) + "000\0";
    int fd = open(fname.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        std::cerr << "Failed to open output file." << std::endl;
        exit(1);
    }
    if (write(fd, header.c_str(), header.length() + 1) < 0) {
        std::cerr << "Failed to write header to output file." << std::endl;
        exit(1);
    }
    __dynprof_output_fd = fd;
}
