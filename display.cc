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
#include <memory>
#include <unordered_map>
#include <vector>

static void usage() { std::cerr << "Usage: ./display out_dynprof.*" << std::endl; }

#define HEADER_SIZE 11

static char expected_header[HEADER_SIZE] = "DYNPROF:1\0";

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

struct FuncCall {
     struct timespec time;
};

typedef std::pair<FuncCall, FuncCall> CallPair;
typedef std::unordered_map<int, CallPair> CallMap;
typedef std::unordered_map<std::string, CallMap> FuncMap;

static double elapsed_time(CallPair calls) {
    struct timespec before = calls.first.time, after = calls.second.time;
    if(after.tv_nsec > before.tv_nsec) {
        return (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
    } else {
        return (after.tv_sec - before.tv_sec - 1) + (after.tv_nsec - before.tv_nsec + 1e9) / 1e9;
    }

}

static void process_output(FuncMap funcs) {
    std::cerr << "%\tcummulative\tself" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\tcalls\tname" << std::endl;
    for(auto func: funcs) {
        double total = 0;
        for(auto call: func.second) {
            total += elapsed_time(call.second);
        }
        std::cerr << "FOO" << "\t" << "FOO" << "\t\t" << total << "\t" << func.second.size() << "\t" << func.first << std::endl;
    }
}

static int read_file(char* fname) {
    int ret = 0, id = 0;
    char* name = nullptr;
    size_t name_len = 0;
    std::unique_ptr<FuncMap> funcs(new FuncMap);
    FILE* f = fopen(fname, "r");
    if (!f) {
        std::cerr << "Failed to open: " << fname << std::endl;
        return -1;
    }
    char header[HEADER_SIZE];
    if (!fgets(header, HEADER_SIZE, f)) {
        std::cerr << "Failed to read header" << std::endl;
        ret = -1;
        goto out;
    }
    if (strcmp(header, expected_header)) {
        std::cerr << "Invalid header:" << header << std::endl;
        ret = -1;
        goto out;
    }
    bool type;
    struct timespec t;
    memset(&t, 0, sizeof(struct timespec));
    while (true) {
        if (getdelim(&name, &name_len, '\0', f) < 0) {
            if (feof(f)) {
                goto out;
            }
            std::cerr << "Could not read name" << std::endl;
            ret = -1;
            goto out;
        }
        if (!read_obj(f, &type, sizeof(bool))) {
            std::cerr << "Could not read type" << std::endl;
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
        if(funcs->count(name) == 0) {
          funcs->insert(std::make_pair(std::string(name), CallMap()));
        }
        if(funcs->at(name).count(id) == 0) {
          funcs->at(name).insert(std::make_pair(id, std::make_pair(FuncCall{}, FuncCall{})));
        }
        if(type) { // after
          funcs->at(name).at(id).second.time = t;
        } else {
          funcs->at(name).at(id).first.time = t;
        }
    }
out:
    free(name);
    fclose(f);
    if(ret == 0) {
        std::cerr << "Profiling Summary from " << fname << std::endl;
        process_output(*funcs);
    }
    return ret;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return -1;
    }
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        ret = read_file(argv[i]);
        if (ret) {
            return ret;
        }
    }
    return ret;
}
