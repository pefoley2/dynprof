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

#include <iostream>
#include <cstdio>
#include <cstring>

static void usage() { std::cerr << "Usage: ./display out_dynprof.*" << std::endl; }

static char expected_header[] = "DYNPROF:1\0";

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return -1;
    }
    FILE* f = fopen(argv[1], "r");
    if(!f) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return -1;
    }
    char header[10];
    if(!fgets(header, 10, f)) {
        std::cerr << "Failed to read header" << std::endl;
        return -1;
    }
    if(strcmp(header, expected_header)) {
        std::cerr << "Invalid header:" << header << std::endl;
        return -1;
    }
    std::cerr << "Profiling Summary:" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\t\tcalls\tname" << std::endl;
    char name[500+1];
    struct timespec t;
    while(!feof(f)) {
        if(fscanf(f, "%s500", name) < 0) {
          std::cerr << "Could not read name" << std::endl;
          return -1;
        }
        fread(&t, sizeof(struct timespec), 1, f);
        if(ferror(f)) {
          std::cerr << "Could not read before" << std::endl;
          return -1;
        }
        std::cerr << name << ":" << t.tv_sec << ":" << t.tv_nsec << std::endl;
    }
    fclose(f);
    return 0;
}
