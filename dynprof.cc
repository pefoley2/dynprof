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

// PCH
#include "dyninst.h"
#include "dynprof.h"

#include <unordered_map>

#define DEFAULT_ENTRY_POINT "main"

unique_ptr<string> get_path(char* exe) {
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
    char* resolved_path = realpath(exe, nullptr);
    if (resolved_path) {
        fullpath->assign(resolved_path);
        if (access(fullpath->c_str(), F_OK) == 0) {
            return fullpath;
        }
    }
    return nullptr;
}


const char** get_params(int argc, char* argv[]) {
    // FIXME: make more idiomatically c++
    int nargs = argc > 2 ? argc - 1 : 2;
    unsigned int paramlen = static_cast<unsigned int>(nargs) * sizeof(char*);
    char** params = static_cast<char**>(malloc(paramlen));
    if (argc > 2) {
        for (int i = 1; i < nargs; i++) {
            size_t arglen = strlen(argv[i + 1]);
            params[i] = static_cast<char*>(malloc(arglen + 1));
            memset(params[i], 0, arglen + 1);
            strncpy(params[i], argv[i + 1], arglen + 1);
        }
        params[nargs] = nullptr;
    } else {
        params[nargs - 1] = nullptr;
    }
    return const_cast<const char**>(params);
}

unique_ptr<vector<BPatch_function*>> get_entry_points(BPatch_process* proc) {
    unique_ptr<vector<BPatch_function*>> funcs(new vector<BPatch_function*>);
    proc->getImage()->findFunction(DEFAULT_ENTRY_POINT, *funcs);
    return funcs;
}

void enum_subroutines(BPatch_function func) {
    unique_ptr<vector<BPatch_point*>> subroutines(func.findPoint(BPatch_subroutine));
    if (subroutines) {
        for (auto subroutine : *subroutines) {
            BPatch_function* subfunc = subroutine->getCalledFunction();
            if (subfunc) {
                cout << "sub:" << subfunc->getName() << endl;
                // FIXME: stack overflow
                // enum_subroutines(*subfunc);
            } else {
                cout << "no called func found for:" << func.getName() << endl;
            }
        }
    } else {
        cout << "no subroutines found for func:" << func.getName() << endl;
    }
}

void hook_functions(BPatch_process* proc) {
    unique_ptr<vector<BPatch_function*>> functions = get_entry_points(proc);
    for (auto func : *functions) {
        cout << "func:" << func->getName() << endl;
        enum_subroutines(*func);
    }
}

void code_discover(BPatch_Vector<BPatch_function*>& newFuncs,
                   BPatch_Vector<BPatch_function*>& modFuncs) {
    for (auto func : newFuncs) {
        cout << "new:" << func->getName() << endl;
    }
    for (auto func : modFuncs) {
        cout << "mod:" << func->getName() << endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./dyninst [program] arg1,arg2,arg3...\n");
        return 1;
    }
    unique_ptr<string> path = get_path(argv[1]);
    if (!path) {
        printf("%s not found in path\n", argv[1]);
        return 1;
    }
    const char** params = get_params(argc, argv);
    params[0] = path->c_str();
    BPatch bpatch;
    bpatch.registerCodeDiscoveryCallback(code_discover);
    unique_ptr<BPatch_process> app(bpatch.processCreate(path->c_str(), params));
    if (!app) {
        printf("Failed to start %s\n", path->c_str());
        return 1;
    } else {
        printf("Successfully started.\n");
    }
    hook_functions(app.get());
    app->continueExecution();
    while (!app->isTerminated()) {
        bpatch.waitForStatusChange();
    }
    return 0;
}
