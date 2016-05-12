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

#include "display.h"

constexpr char Output::expected_header[];

static void usage() { std::cerr << "Usage: ./display out_dynprof.*" << std::endl; }

bool Output::read_obj(FILE* f, void* ptr, size_t len) {
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

double Output::elapsed_time(CallPair calls) {
    struct timespec before = calls.first.time, after = calls.second.time;
    double elapsed;
    if (after.tv_nsec > before.tv_nsec) {
        elapsed = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
    } else {
        elapsed = (after.tv_sec - before.tv_sec - 1) + (after.tv_nsec - before.tv_nsec + 1e9) / 1e9;
    }
    if (elapsed < 0) {
        // FIXME: what is going on here?
        std::cerr << "ERROR: invalid elapsed time of " << elapsed << std::endl;
        // exit(-1);
    }
    return elapsed;
}

void Output::process_output(FuncMap funcs) {
    double total = elapsed_time(funcs[DEFAULT_ENTRY_POINT].at(0));
    std::vector<FuncOutput> output;
    for (auto func : funcs) {
        double ftime = 0;
        for (auto call : func.second) {
            ftime += elapsed_time(call.second);
        }
        output.push_back({(ftime / total * 100), ftime, func.second.size(), func.first});
    }
    std::cerr << "%\tcummulative\tself" << std::endl;
    std::cerr << "time\tseconds\t\tseconds\t\tcalls\tname" << std::endl;
    std::sort(output.begin(), output.end());
    for (auto func : output) {
        // FIXME: figure out what's going on here.
        if (func.percent < 0) {
            func.percent = func.elapsed = -1;
        }
        std::cerr << std::fixed << std::setprecision(2) << func.percent << "\t"
                  << std::setprecision(5) << func.elapsed << "\t\t"
                  << "TODO"
                  << "\t\t" << func.calls << "\t" << func.name << std::endl;
    }
}

int Output::process_file(std::string fname) {
    int ret = 0, id = 0;
    char* name = nullptr;
    size_t name_len = 0;
    bool type = false;
    std::unique_ptr<FuncMap> funcs(new FuncMap);
    FILE* f = fopen(fname.c_str(), "r");
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
        if (funcs->count(name) == 0) {
            funcs->insert(std::make_pair(std::string(name), CallMap()));
        }
        if (funcs->at(name).count(id) == 0) {
            funcs->at(name).insert(std::make_pair(id, std::make_pair(FuncCall{}, FuncCall{})));
        }
        if (type) {  // after
            funcs->at(name).at(id).second.time = t;
        } else {
            funcs->at(name).at(id).first.time = t;
        }
    }
out:
    free(name);
    fclose(f);
    if (ret == 0) {
        std::cerr << "Profiling Summary from " << fname << std::endl;
        process_output(*funcs);
    }
    return ret;
}

int Output::process() {
    int ret = 0;
    for (auto file : args) {
        ret = process_file(file);
        if (ret) {
            return ret;
        }
    }
    return ret;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return -1;
    }
    std::vector<std::string> args(argv, argv + argc);
    // We don't want argv[0]
    args.erase(args.begin());
    Output out(args);
    return out.process();
}
