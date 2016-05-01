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

using namespace std::chrono;

static int num_funcs;
static FuncInfo* funcs;

void __dynprof_register_handler(int len) {
    if (len < 0) {
        std::cerr << "Invalid number of functions: " << len << std::endl;
        exit(1);
    }
    funcs = new FuncInfo[len];
    if (atexit(exit_handler)) {
        std::cerr << "Failed to register atexit handler." << std::endl;
        exit(1);
    }
}

/*
void elapsed_time(struct timespec* before, struct timespec* after, double* output) {
    std::cerr << output << std::endl;
    std::cerr << *output << std::endl;
    //FIXME: *output = 420.5;
    nanoseconds before_c =
        seconds(before->tv_sec) + nanoseconds(before->tv_nsec);
    nanoseconds after_c =
        seconds(after->tv_sec) + nanoseconds(after->tv_nsec);
    nanoseconds elapsed = after_c - before_c;
    double bob = elapsed.count() / duration_cast<nanoseconds>(seconds(1)).count();
    std::cerr << "foo:" << bob << ":" << elapsed.count() << std::endl;
    return bob;
}
*/

void exit_handler() {
    std::cerr << "Profiling Summary:" << std::endl;
    std::cerr << "%\tcumulative\tself" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\t\tcalls\tname" << std::endl;
    for (int i = 0; i < num_funcs; i++) {
        std::cerr << funcs[i] << std::endl;
    }
    delete[] funcs;
}
