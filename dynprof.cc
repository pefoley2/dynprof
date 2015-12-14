/*
 * DynProf, an Dyninst-based dynamic profiler.
 * Copyright (C) 2015 Peter Foley
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

#include "dynprof.h"

#define DEFAULT_ENTRY_POINT "main"

unique_ptr<string> get_path(string exe) {
    // FIXME: make more idiomatically c++
    char* path = getenv("PATH");
    char* dir = strtok(path, ":");
    unique_ptr<string> fullpath(new string);
    while (dir) {
        fullpath->assign(dir);
        fullpath->append("/");
        fullpath->append(exe);
        if (access(fullpath->c_str(), F_OK) == 0) {
            return fullpath;
        }
        dir = strtok(nullptr, ":");
    }
    char* resolved_path = realpath(exe.c_str(), nullptr);
    if (resolved_path) {
        fullpath->assign(resolved_path);
        if (access(fullpath->c_str(), F_OK) == 0) {
            return fullpath;
        }
    }
    return nullptr;
}

const char** get_params(vector<string> args) {
    char** params = static_cast<char**>(malloc((args.size() + 1) * sizeof(char*)));
    // Skip the exe, that's resolved by get_path
    for (size_t i = 1; i < args.size(); i++) {
        size_t arglen = args[i].size() + 1;
        params[i] = static_cast<char*>(malloc(arglen));
        memset(params[i], 0, arglen);
        strcpy(params[i], args[i].c_str());
    }
    params[args.size()] = nullptr;
    return const_cast<const char**>(params);
}

unique_ptr<vector<BPatch_function*>> DynProf::get_entry_point() {
    unique_ptr<vector<BPatch_function*>> funcs(new vector<BPatch_function*>);
    // Should only return one function.
    app->getImage()->findFunction(DEFAULT_ENTRY_POINT, *funcs);
    return funcs;
}

void DynProf::enum_subroutines(BPatch_function* func) {
    unique_ptr<vector<BPatch_point*>> subroutines(func->findPoint(BPatch_subroutine));
    if (!subroutines) {
        return;
    }
    for (auto subroutine : *subroutines) {
        BPatch_function* subfunc = subroutine->getCalledFunction();
        if (subfunc) {
            // Ignore internal functions.
            if (subfunc->getName().compare(0, 2, "__") == 0) {
                // cout << "skip:" << subfunc->getName() << endl;
            } else if (func_map.count(subfunc->getName())) {
                // cout << "already enumed:" << subfunc->getName() << endl;
            } else {
                cout << "sub:" << subfunc->getName() << endl;
                func_map[subfunc->getName()] = subfunc->getBaseAddr();
                enum_subroutines(subfunc);
            }
        }
    }
}

void DynProf::hook_functions() {
    unique_ptr<vector<BPatch_function*>> functions = get_entry_point();
    for (auto func : *functions) {
        cout << "func:" << func->getName() << endl;
        enum_subroutines(func);
    }
}

/*
void code_discover(BPatch_Vector<BPatch_function*>& newFuncs,
        BPatch_Vector<BPatch_function*>& modFuncs) {
    for (auto func : newFuncs) {
        cout << "new:" << func->getName() << endl;
    }
    for (auto func : modFuncs) {
        cout << "mod:" << func->getName() << endl;
    }
}
*/

void DynProf::start() {
    // bpatch.registerCodeDiscoveryCallback(code_discover);
    app = bpatch.processCreate(path.c_str(), params);
    if (!app) {
        cerr << "Failed to start " << path << endl;
        exit(1);
    } else {
        cerr << "Successfully started profiling" << endl;
    }
    hook_functions();
    app->continueExecution();
}

void DynProf::waitForExit() {
    while (!app->isTerminated()) {
        bpatch.waitForStatusChange();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./dyninst [program] arg1,arg2,arg3...\n");
        return 1;
    }
    vector<string> args(argv, argv + argc);
    unique_ptr<string> path = get_path(args[1]);
    if (!path) {
        cerr << args[1] << " not found in path" << endl;
        return 1;
    }
    args.erase(args.begin());
    const char** params = get_params(args);
    // Re-add original path.
    params[0] = path->c_str();
    DynProf prof(*path, params);
    prof.start();
    prof.waitForExit();
    return 0;
}
