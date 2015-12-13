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

#include <BPatch.h>
#include <BPatch_function.h>
#include <BPatch_point.h>

#include <unordered_map>

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

BPatch bpatch;

const char** get_params(int argc, char* argv[]) {
    int nargs = argc > 2 ? argc - 1 : 2;
    char** params = (char**)malloc(nargs * sizeof(char*));
    if (argc > 2) {
        for (int i = 1; i < nargs; i++) {
            int arglen = strlen(argv[i + 1]);
            params[i] = (char*)malloc(arglen + 1);
            memset(params[i], 0, arglen + 1);
            strncpy(params[i], argv[i + 1], arglen + 1);
        }
        params[nargs] = NULL;
    } else {
        params[nargs - 1] = NULL;
    }
    return (const char**)params;
}

typedef unordered_map<dynthread_t, BPatch_function*> func_map;

unique_ptr<func_map> get_entry_points(BPatch_process* proc) {
    unique_ptr<vector<BPatch_thread*>> threads(new vector<BPatch_thread*>);
    unique_ptr<func_map> functions(new func_map);
    proc->getThreads(*threads);
    for (BPatch_thread* th : *threads) {
        // getCallStack?
        functions->emplace(th->getTid(), th->getInitialFunc());
    }
    return functions;
}

void hook_functions(BPatch_process* proc) {
    unique_ptr<func_map> functions = get_entry_points(proc);
    for (auto func : *functions) {
        cout << func.first << ":" << func.second->getName() << endl;
        unique_ptr<vector<BPatch_point*>> subroutines(func.second->findPoint(BPatch_subroutine));
        if (subroutines) {
            for (auto subroutine : *subroutines) {
                cout << subroutine->getCalledFunctionName() << endl;
            }
        } else {
            cout << "no subroutines found." << endl;
        }
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
