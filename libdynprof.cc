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

static int output_fd;

void __dynprof_register_handler() {
    if (atexit(exit_handler)) {
        std::cerr << "Failed to register atexit handler." << std::endl;
        exit(1);
    }
    std::string fname = "out_dynprof." + std::to_string(getpid());
    std::string header = "DYNPROF:" + std::to_string(OUTPUT_VERSION) + "\0";
    int fd = open(fname.c_str(), O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
    std::cerr << "memes: " << fd << std::endl;
    if(fd < 0) {
        std::cerr << "Failed to open output file." << std::endl;
        exit(1);
    }
    if(write(fd, header.c_str(), header.length()) < 0) {
        std::cerr << "Failed to write header to output file." << std::endl;
        exit(1);
    }
    output_fd = fd;
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

/*
std::ostream& operator<<(std::ostream& out, const FuncOutput& info) {
    out << "name: " << info.name
        << "count: " << info.count << ", before: " << info.before << ", after: " << info.after;
    return out;
}

void copy_func_info(int count, FuncOutput* out) {
    std::cerr << "addr:" << out << std::endl;
    out->count = 42;
}
*/

void exit_handler() {
    std::cerr << "memes: " << output_fd << std::endl;
    if(close(output_fd) < 0) {
        std::cerr << "Failed to close output file." << std::endl;
        exit(1);
    }
  /*  std::cerr << "%\tcumulative\tself" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\t\tcalls\tname" << std::endl;
    for (int i = 0; i < MAX_NUM_FUNCS; i++) {
        if(funcs[i].count) {
          std::cerr << funcs[i] << std::endl;
        }
    }*/
}
