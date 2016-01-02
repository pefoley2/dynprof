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

unique_ptr<string> get_path(string exe) {
    // FIXME: make more idiomatically c++
    // string.find_first_of() instead of strtok
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
        strncpy(params[i], args[i].c_str(), args[i].size() + 1);
    }
    params[args.size()] = nullptr;
    return const_cast<const char**>(params);
}

void FuncInfo::addChild(BPatch_function* func) { children.push_back(func); }

BPatch_variableExpr FuncInfo::getCount() { return *count; }

void DynProf::recordFunc(BPatch_function* func) {
    BPatch_variableExpr* count = app->malloc(*app->getImage()->findType("int"));
    func_map.insert(make_pair(func, new FuncInfo(count)));
}

void DynProf::enum_subroutines(BPatch_function* func) {
    // Already visited.
    if (func_map.count(func)) {
        return;
    }
    // Register entry/exit snippets.
    recordFunc(func);
    createSnippets(func);
    unique_ptr<vector<BPatch_point*>> subroutines(func->findPoint(BPatch_subroutine));
    if (!subroutines) {
        // This function doesn't call any others.
        return;
    }
    for (auto subroutine : *subroutines) {
        BPatch_function* subfunc = subroutine->getCalledFunction();
        if (subfunc) {
            // TODO: deal with library functions.
            if (subfunc->isSharedLib()) {
                // cout << "skip:" << subfunc->getName() << endl;
            } else {
                cout << "sub:" << subfunc->getName() << endl;
                func_map[func]->addChild(subfunc);
                enum_subroutines(subfunc);
            }
        }
    }
}

unique_ptr<vector<BPatch_function*>> DynProf::get_entry_point() {
    unique_ptr<vector<BPatch_function*>> funcs(new vector<BPatch_function*>);
    // Should only return one function.
    app->getImage()->findFunction(DEFAULT_ENTRY_POINT, *funcs);
    return funcs;
}

void DynProf::hook_functions() {
    unique_ptr<vector<BPatch_function*>> functions = get_entry_point();
    for (auto func : *functions) {
        cerr << "func:" << func->getName() << endl;
        enum_subroutines(func);
    }
}

void DynProf::createSnippets(BPatch_function* func) {
    unique_ptr<vector<BPatch_point*>> entry_point(func->findPoint(BPatch_entry));
    if (!entry_point || entry_point->size() == 0) {
        cerr << "Could not find entry point for " << func->getName() << endl;
        return;
    }

    unique_ptr<vector<BPatch_point*>> exit_point(func->findPoint(BPatch_exit));
    if (!exit_point || exit_point->size() == 0) {
        cerr << "Could not find exit point for " << func->getName() << endl;
        return;
    }

    vector<BPatch_snippet*> entry_args;
    entry_args.push_back(new BPatch_constExpr("Entering %s\n"));
    entry_args.push_back(new BPatch_constExpr(func->getName().c_str()));
    vector<BPatch_snippet*> exit_args;
    exit_args.push_back(new BPatch_constExpr("Exiting %s\n"));
    exit_args.push_back(new BPatch_constExpr(func->getName().c_str()));

    vector<BPatch_function*> printf_funcs;
    // printf isn't profilable.
    app->getImage()->findFunction("printf", printf_funcs, true, true, true);
    if (printf_funcs.size() != 1) {
        cerr << "Could not find printf" << endl;
        return;
    }
    vector<BPatch_function*> clock_funcs;
    app->getImage()->findFunction("clock_gettime", clock_funcs);
    // TODO: keep track of the time...

    BPatch_arithExpr incCount(
        BPatch_assign, func_map[func]->getCount(),
        BPatch_arithExpr(BPatch_plus, func_map[func]->getCount(), BPatch_constExpr(1)));

    BPatch_funcCallExpr entry_snippet(*printf_funcs.at(0), entry_args);
    BPatch_funcCallExpr exit_snippet(*printf_funcs.at(0), exit_args);

    vector<BPatch_snippet*> entry_vec{&incCount, &entry_snippet};
    BPatch_sequence entry_seq(entry_vec);

    app->beginInsertionSet();
    app->insertSnippet(entry_seq, *entry_point->at(0), BPatch_callBefore);
    app->insertSnippet(exit_snippet, *exit_point->at(0), BPatch_callAfter);
    if (!app->finalizeInsertionSet(true)) {
        cerr << "Failed to insert snippets around " << func->getName() << endl;
        return;
    }
}

void DynProf::start() {
    cerr << "Preparing to profile " << path << endl;
    app = bpatch.processCreate(path.c_str(), params);
    if (app->isMultithreadCapable()) {
        cerr << "Multithreading is not yet handled." << endl;
        exit(1);
    }
    if (!app) {
        cerr << "Failed to load " << path << endl;
        exit(1);
    } else {
        cerr << "Process loaded; Enumerating functions" << endl;
    }
    hook_functions();
    cerr << "Resuming execution" << endl;
    app->continueExecution();
}

int DynProf::waitForExit() {
    while (!app->isTerminated()) {
        bpatch.waitForStatusChange();
    }
    return app->getExitCode();
}

void DynProf::printCallCounts() {
    int count;
    for (auto& func : func_map) {
        func.second->getCount().readValue(&count);
        if (count) {
            cerr << func.first->getName() << ":" << count << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: ./dyninst [program] arg1,arg2,arg3..." << endl;
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
    // set argv[0] to the original path.
    params[0] = path->c_str();
    DynProf prof(*path, params);
    prof.start();
    int status = prof.waitForExit();
    prof.printCallCounts();
    cerr << "Program exited with status: " << status << endl;
    return status;
}
