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

#include "dynprof.h"

void FuncInfo::addChild(BPatch_function* func) { children.push_back(func); }

void DynProf::recordFunc(BPatch_function* func) {
    BPatch_variableExpr* count = app->malloc(*app->getImage()->findType("int"));
    BPatch_variableExpr* before = app->malloc(*timespec_struct);
    BPatch_variableExpr* after = app->malloc(*timespec_struct);
    func_map().insert(make_pair(func, new FuncInfo(count, before, after)));
}

void DynProf::enum_subroutines(BPatch_function* func) {
    // Already visited.
    if (func_map().count(func)) {
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
            // TODO(peter): deal with library functions.
            if (subfunc->isSharedLib()) {
                // cout << "skip:" << subfunc->getName() << endl;
            } else {
                cout << "sub:" << subfunc->getName() << endl;
                func_map()[func]->addChild(subfunc);
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

bool DynProf::createBeforeSnippet(BPatch_function* func) {
    unique_ptr<vector<BPatch_point*>> entry_point(func->findPoint(BPatch_entry));
    if (!entry_point || entry_point->size() != 1) {
        cerr << "Could not find entry point for " << func->getName() << endl;
        return false;
    }

    vector<BPatch_snippet*> entry_args;
    entry_args.push_back(new BPatch_constExpr("Entering %s\n"));
    entry_args.push_back(new BPatch_constExpr(func->getName().c_str()));

    BPatch_arithExpr incCount(
        BPatch_assign, *func_map()[func]->count,
        BPatch_arithExpr(BPatch_plus, *func_map()[func]->count, BPatch_constExpr(1)));

    // TODO(peter): remove this for final product.
    vector<BPatch_function*> printf_funcs;
    // printf isn't profilable.
    app->getImage()->findFunction("printf", printf_funcs, true, true, true);
    if (printf_funcs.size() != 1) {
        cerr << "Could not find printf" << endl;
        return false;
    }

    BPatch_funcCallExpr entry_snippet(*printf_funcs.at(0), entry_args);

    vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(func_map()[func]->before);
    BPatch_funcCallExpr before_record(*clock_func, clock_args);

    vector<BPatch_snippet*> entry_vec{&incCount, &entry_snippet};
    BPatch_sequence entry_seq(entry_vec);
    app->insertSnippet(entry_seq, *entry_point->at(0), BPatch_callBefore);
    // FIXME: why does this break when added to the same seq as the other snippets?
    app->insertSnippet(before_record, *entry_point->at(0), BPatch_callBefore);
    return true;
}

bool DynProf::createAfterSnippet(BPatch_function* func) {
    unique_ptr<vector<BPatch_point*>> exit_point(func->findPoint(BPatch_exit));
    if (!exit_point || exit_point->size() != 1) {
        cerr << "Could not find exit point for " << func->getName() << endl;
        return false;
    }

    vector<BPatch_snippet*> exit_args;
    exit_args.push_back(new BPatch_constExpr("Exiting %s\n"));
    exit_args.push_back(new BPatch_constExpr(func->getName().c_str()));

    // TODO(peter): remove this for final product.
    vector<BPatch_function*> printf_funcs;
    // printf isn't profilable.
    app->getImage()->findFunction("printf", printf_funcs, true, true, true);
    if (printf_funcs.size() != 1) {
        cerr << "Could not find printf" << endl;
        return false;
    }

    BPatch_funcCallExpr exit_snippet(*printf_funcs.at(0), exit_args);

    vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(func_map()[func]->after);
    BPatch_funcCallExpr after_record(*clock_func, clock_args);

    app->insertSnippet(exit_snippet, *exit_point->at(0), BPatch_callAfter);
    // FIXME: why does this break when added to a seq with other snippets?
    app->insertSnippet(after_record, *exit_point->at(0), BPatch_callAfter);
    return true;
}

void DynProf::createSnippets(BPatch_function* func) {
    app->beginInsertionSet();

    if (!createBeforeSnippet(func) || !createAfterSnippet(func)) {
        return;
    }

    if (!app->finalizeInsertionSet(true)) {
        cerr << "Failed to insert snippets around " << func->getName() << endl;
    }
}

void DynProf::create_structs() {
    vector<char*> field_names{const_cast<char*>("tv_sec"), const_cast<char*>("tv_nsec")};
    vector<BPatch_type*> field_types{
        app->getImage()->findType("long"),  // time_t is ultimately a typedef to long
        app->getImage()->findType("long")};
    timespec_struct = bpatch.createStruct("timespec", field_names, field_types);
    if (!timespec_struct) {
        cerr << "Failed to create struct." << endl;
        exit(1);
    }
}

void DynProf::find_funcs() {
    vector<BPatch_function*> clock_funcs;
    app->getImage()->findFunction("clock_gettime", clock_funcs);
    if (clock_funcs.size() != 1) {
        cerr << "Could not find clock_gettime" << endl;
        return;
    }
    clock_func = clock_funcs.at(0);
}

void DynProf::start() {
    cerr << "Preparing to profile " << *path << endl;
    app = bpatch.processCreate(path->c_str(), params);
    doSetup();
    if (static_cast<BPatch_process*>(app)->isMultithreadCapable()) {
        // TODO(peter): handle entry points other than main().
        // app->getThreads()
        cerr << "Multithreading is not yet handled." << endl;
        exit(1);
    }
    cerr << "Resuming execution" << endl;
    static_cast<BPatch_process*>(app)->continueExecution();
}

void DynProf::setupBinary() {
    cerr << "Preparing " << *path << " for profiling" << endl;
    app = bpatch.openBinary(path->c_str(), true);
    doSetup();
}

void DynProf::doSetup() {
    if (!app) {
        cerr << "Failed to load " << *path << endl;
        exit(1);
    }
    create_structs();
    find_funcs();
    cerr << "Enumerating functions" << endl;
    hook_functions();
}

int DynProf::waitForExit() {
    while (!static_cast<BPatch_process*>(app)->isTerminated()) {
        bpatch.waitForStatusChange();
    }
    int status = static_cast<BPatch_process*>(app)->getExitCode();
    cerr << "Program exited with status: " << status << endl;
    return status;
}

bool DynProf::writeOutput() {
    string out_file = executable + "_dynprof";
    bool status = static_cast<BPatch_binaryEdit*>(app)->writeFile(out_file.c_str());
    if (status) {
        cerr << "Modified binary written to: " << out_file << endl;
    } else {
        cerr << "Failed to write modified binary to: " << out_file << endl;
    }
    return !status;
}

void DynProf::printCallCounts() {
    int count;
    for (auto& func : func_map()) {
        func.second->count->readValue(&count);
        if (count) {
            cerr << count << ":" << func.first->getName() << endl;
        }
    }
}

double DynProf::elapsed_time(struct timespec* before, struct timespec* after) {
    chrono::nanoseconds before_c =
        chrono::seconds(before->tv_sec) + chrono::nanoseconds(before->tv_nsec);
    chrono::nanoseconds after_c =
        chrono::seconds(after->tv_sec) + chrono::nanoseconds(after->tv_nsec);
    chrono::nanoseconds elapsed = after_c - before_c;
    return elapsed.count();
}

void DynProf::printElapsedTime() {
    struct timespec* before = static_cast<struct timespec*>(malloc(sizeof(struct timespec)));
    struct timespec* after = static_cast<struct timespec*>(malloc(sizeof(struct timespec)));
    memset(before, 0, sizeof(struct timespec));
    memset(after, 0, sizeof(struct timespec));
    for (auto& func : func_map()) {
        func.second->before->readValue(before);
        func.second->after->readValue(after);
        cerr << fixed << elapsed_time(before, after) << ":" << func.first->getName() << endl;
    }
    free(before);
    free(after);
}

void DynProf::ExitCallback(BPatch_thread* /*unused*/, BPatch_exitType exit_type) {
    if (exit_type != ExitedNormally) {
        return;
    }
    printCallCounts();
    printElapsedTime();
}

// Needed for the ExitCallback
function_mapping& func_map() {
    static function_mapping* funcs = new function_mapping();
    return *funcs;
}
