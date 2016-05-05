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

static bool read_obj(FILE* f, void* ptr, size_t len) {
    if (ferror(f)) {
        return false;
    }
    clearerr(f);
    if (fread(ptr, len, 1, f) != 1) {
        return false;
    }
    if (ferror(f)) {
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return -1;
    }
    int ret = 0;
    char* name = nullptr;
    FILE* f = fopen(argv[1], "r");
    if (!f) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return -1;
    }
    char header[10];
    if (!fgets(header, 10, f)) {
        std::cerr << "Failed to read header" << std::endl;
        ret = -1;
        goto out;
    }
    if (strcmp(header, expected_header)) {
        std::cerr << "Invalid header:" << header << std::endl;
        ret = -1;
        goto out;
    }
    std::cerr << "Profiling Summary:" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\t\tcalls\tname" << std::endl;
    struct timespec t;
    size_t name_len;
    int id;
    while (!feof(f)) {
        if (!read_obj(f, &name_len, sizeof(size_t))) {
            std::cerr << "Could not read name length" << std::endl;
            ret = -1;
            goto out;
        }
        name = static_cast<char*>(malloc(name_len));
        if (!read_obj(f, name, name_len)) {
            std::cerr << "Could not read name" << std::endl;
            ret = -1;
            goto out;
        }
        if (!read_obj(f, &id, sizeof(int))) {
            std::cerr << "Could not read id" << std::endl;
            ret = -1;
            goto out;
        }
        if (!read_obj(f, &t, sizeof(struct timespec))) {
            std::cerr << "Could not read timespec" << std::endl;
            ret = -1;
            goto out;
        }
        std::cerr << name << ":" << id << ":" << t.tv_sec << ":" << t.tv_nsec << std::endl;
        free(name);
        name = nullptr;
    }
out:
    free(name);
    fclose(f);
    return ret;
}
