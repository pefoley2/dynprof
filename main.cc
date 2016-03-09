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

#include <unistd.h>   // for F_OK

#include "dynprof.h"  // for DynProf

unique_ptr<string> get_path(string exe);
const char** get_params(vector<string> args);
void usage();

unique_ptr<string> get_path(string exe) {
    unique_ptr<string> path(new string);
    path->assign(getenv("PATH"));
    unique_ptr<string> fullpath(new string);
    size_t current = 0;
    size_t offset;
    do {
        offset = path->find_first_of(":", current);
        fullpath->assign(path->substr(current, offset - current));
        fullpath->append("/");
        fullpath->append(exe);
        if (access(fullpath->c_str(), F_OK) == 0) {
            return fullpath;
        }
        current = offset + 1;
    } while (offset != string::npos);

    fullpath->assign(resolve_path(exe));

    return fullpath;
}

const char** get_params(vector<string> args) {
    char** params = static_cast<char**>(malloc((args.size() + 1) * sizeof(char*)));
    // Skip the exe, that's resolved by get_path
    for (size_t i = 1; i < args.size(); i++) {
        size_t arglen = args[i].size() + 1;
        params[i] = static_cast<char*>(malloc(arglen));
        memset(params[i], 0, arglen);
        strncpy(params[i], args[i].c_str(), args[i].size() + 1);
    }
    params[args.size()] = nullptr;
    return const_cast<const char**>(params);
}

void usage() { cerr << "Usage: ./dynprof (--write) [program] arg1,arg2,arg3..." << endl; }

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }
    vector<string> args(argv, argv + argc);
    bool binary_edit = false;
    if (args[1] == "--write") {
        if (argc < 3) {
            usage();
            return 1;
        }
        binary_edit = true;
        args.erase(args.begin());
    }
    unique_ptr<string> path = get_path(args[1]);
    if (!path) {
        cerr << args[1] << " not found in path" << endl;
        return 1;
    }
    args.erase(args.begin());
    const char** params = get_params(args);
    // set argv[0] to the original path.
    params[0] = path->c_str();
    DynProf prof(std::move(path), params);
    if (binary_edit) {
        prof.setupBinary();
        return prof.writeOutput();
    }

    prof.start();
    return prof.waitForExit();
}
