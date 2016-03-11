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

std::string resolve_path(std::string file) {
    char resolved_path[PATH_MAX];
    if (realpath(file.c_str(), resolved_path)) {
        if (access(resolved_path, F_OK) == 0) {
            return std::string(resolved_path);
        }
    }
    return file;
}

void FuncInfo::addChild(BPatch_function* func) { children.push_back(func); }

void DynProf::recordFunc(BPatch_function* func) {
    BPatch_variableExpr* count = app->malloc(*app->getImage()->findType("int"));
    BPatch_variableExpr* before = app->malloc(*timespec_struct);
    BPatch_variableExpr* after = app->malloc(*timespec_struct);
    func_map.insert(std::make_pair(func, new FuncInfo(count, before, after)));
}

void DynProf::enum_subroutines(BPatch_function* func) {
    // Already visited.
    if (func_map.count(func)) {
        return;
    }
    recordFunc(func);
    if (func->getName() != DEFAULT_ENTRY_POINT) {
        // Register entry/exit snippets.
        createSnippets(func);
    }
    std::unique_ptr<std::vector<BPatch_point*>> subroutines(func->findPoint(BPatch_subroutine));
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
                std::cout << "sub:" << subfunc->getName() << std::endl;
                func_map[func]->addChild(subfunc);
                enum_subroutines(subfunc);
            }
        }
    }
}

BPatch_function* DynProf::get_function(std::string name, bool uninstrumentable) {
    std::unique_ptr<std::vector<BPatch_function*>> funcs(new std::vector<BPatch_function*>);
    // Should only return one function.
    app->getImage()->findFunction(name.c_str(), *funcs, true, true, uninstrumentable);
    if (funcs->size() != 1) {
        std::cerr << "Failed to find exactly one match for: " << name << std::endl;
        shutdown();
    }
    return funcs->at(0);
}

void DynProf::hook_functions() {
    BPatch_function* func = get_function(DEFAULT_ENTRY_POINT);
    std::cerr << "entry point:" << func->getName() << std::endl;
    enum_subroutines(func);
    registerCleanupSnippet();
}

bool DynProf::createBeforeSnippet(BPatch_function* func) {
    std::unique_ptr<std::vector<BPatch_point*>> entry_points(func->findPoint(BPatch_entry));
    if (!entry_points || entry_points->size() == 0) {
        std::cerr << "Could not find entry point for " << func->getName() << std::endl;
        return false;
    }

    std::vector<BPatch_snippet*> entry_args;
    entry_args.push_back(new BPatch_constExpr("Entering %s\n"));
    entry_args.push_back(new BPatch_constExpr(func->getName().c_str()));

    BPatch_arithExpr incCount(
        BPatch_assign, *func_map[func]->count,
        BPatch_arithExpr(BPatch_plus, *func_map[func]->count, BPatch_constExpr(1)));

    BPatch_funcCallExpr entry_snippet(*printf_func, entry_args);

    std::vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(new BPatch_arithExpr(BPatch_addr, *func_map[func]->before));
    BPatch_funcCallExpr before_record(*clock_func, clock_args);

    std::vector<BPatch_snippet*> entry_vec{&incCount, &entry_snippet, &before_record};
    BPatch_sequence entry_seq(entry_vec);
    for (auto entry_point : *entry_points) {
        app->insertSnippet(entry_seq, *entry_point, BPatch_callBefore);
    }
    return true;
}

bool DynProf::createAfterSnippet(BPatch_function* func) {
    std::unique_ptr<std::vector<BPatch_point*>> exit_points(func->findPoint(BPatch_exit));
    if (!exit_points || exit_points->size() == 0) {
        std::cerr << "Could not find exit point for " << func->getName() << std::endl;
        return false;
    }

    std::vector<BPatch_snippet*> exit_args;
    exit_args.push_back(new BPatch_constExpr("Exiting %s\n"));
    exit_args.push_back(new BPatch_constExpr(func->getName().c_str()));

    BPatch_funcCallExpr exit_snippet(*printf_func, exit_args);

    std::vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(new BPatch_arithExpr(BPatch_addr, *func_map[func]->after));
    BPatch_funcCallExpr after_record(*clock_func, clock_args);

    std::vector<BPatch_snippet*> exit_vec{&exit_snippet, &after_record};
    BPatch_sequence exit_seq(exit_vec);
    for (auto exit_point : *exit_points) {
        app->insertSnippet(exit_seq, *exit_point, BPatch_callAfter);
    }
    return true;
}

void DynProf::registerCleanupSnippet() {
    std::vector<BPatch_function*> exit_funcs;
    app->loadLibrary(resolve_path(HELPER_LIB).c_str());
    app->getImage()->findFunction("__dynprof_register_handler", exit_funcs);
    if (exit_funcs.size() != 1) {
        std::cerr << "Could not find handler registration function." << std::endl;
        shutdown();
    }
    BPatch_funcCallExpr atexit_reg(*exit_funcs[0], {});

    BPatch_function* func = get_function(DEFAULT_ENTRY_POINT);
    std::unique_ptr<std::vector<BPatch_point*>> entry_points(func->findPoint(BPatch_entry));
    if (!entry_points || entry_points->size() == 0) {
        std::cerr << "Could not find entry point for " << func->getName() << std::endl;
        shutdown();
    }
    if (!app->insertSnippet(atexit_reg, *entry_points->at(0), BPatch_callBefore)) {
        std::cerr << "Could not insert atexit snippet." << std::endl;
        shutdown();
    }

    std::vector<BPatch_snippet*> snippets;
    for (auto& child_func : func_map) {
        if (child_func.first->getName() == DEFAULT_ENTRY_POINT) {
            continue;
        }
        std::vector<BPatch_variableExpr*>* components = child_func.second->before->getComponents();
        if (components->size() != 2) {
            std::cerr << "Invalid number of components." << std::endl;
            shutdown();
        }
        // FIXME: use DynC?
        std::vector<BPatch_snippet*> exit_args;
        exit_args.push_back(new BPatch_constExpr("%d:%ld:%ld:%s\n"));
        exit_args.push_back(child_func.second->count);
        exit_args.push_back(components->at(0));
        exit_args.push_back(components->at(1));
        exit_args.push_back(new BPatch_constExpr(child_func.first->getName().c_str()));

        // Only print the summary if the function has been called.
        BPatch_funcCallExpr func_snip(*printf_func, exit_args);
        BPatch_boolExpr count_expr(BPatch_ne, *child_func.second->count, BPatch_constExpr(0));
        snippets.push_back(new BPatch_ifExpr(count_expr, func_snip));
    }
    BPatch_sequence exit_snippet(snippets);

    std::vector<BPatch_object*> objects;
    app->getImage()->getObjects(objects);
    for (auto obj : objects) {
        if (obj->pathName() != *path) {
            continue;
        }
        // FIXME: doesn't always work
        obj->insertFiniCallback(exit_snippet);
    }
}

void DynProf::createSnippets(BPatch_function* func) {
    // FIXME: do one insertion set for all the snippets?
    app->beginInsertionSet();

    if (!createBeforeSnippet(func) || !createAfterSnippet(func)) {
        return;
    }

    if (!app->finalizeInsertionSet(true)) {
        std::cerr << "Failed to insert snippets around " << func->getName() << std::endl;
    }
}

void DynProf::create_structs() {
    std::vector<char*> field_names{const_cast<char*>("tv_sec"), const_cast<char*>("tv_nsec")};
    std::vector<BPatch_type*> field_types{
        app->getImage()->findType("long"),  // time_t is ultimately a typedef to long
        app->getImage()->findType("long")};
    timespec_struct = bpatch.createStruct("timespec", field_names, field_types);
    if (!timespec_struct) {
        std::cerr << "Failed to create struct." << std::endl;
        shutdown();
    }
}

void DynProf::find_funcs() {
    clock_func = get_function("clock_gettime");
    // TODO(peter): remove this for final product.
    printf_func = get_function("printf", true);
}

void DynProf::start() {
    std::cerr << "Preparing to profile " << *path << std::endl;
    app = bpatch.processCreate(path->c_str(), params);
    doSetup();
    if (static_cast<BPatch_process*>(app)->isMultithreadCapable()) {
        // TODO(peter): handle entry points other than main().
        // app->getThreads()
        std::cerr << "Multithreading is not yet handled." << std::endl;
        shutdown();
    }
    std::cerr << "Resuming execution" << std::endl;
    static_cast<BPatch_process*>(app)->continueExecution();
}

void DynProf::setupBinary() {
    std::cerr << "Preparing " << *path << " for profiling" << std::endl;
    app = bpatch.openBinary(path->c_str(), true);
    doSetup();
    update_needed();
}

void DynProf::doSetup() {
    if (!app) {
        std::cerr << "Failed to load " << *path << std::endl;
        shutdown();
    }
    create_structs();
    find_funcs();
    std::cerr << "Enumerating functions" << std::endl;
    hook_functions();
}

int DynProf::waitForExit() {
    while (!static_cast<BPatch_process*>(app)->isTerminated()) {
        bpatch.waitForStatusChange();
    }
    int status = static_cast<BPatch_process*>(app)->getExitCode();
    std::cerr << "Program exited with status: " << status << std::endl;
    return status;
}

void DynProf::update_needed() {
    // We already have a dep to the full path of the helper library,
    // remove this one so we don't need to set LD_LIBRARY_PATH
    std::vector<BPatch_object*> objs;
    app->getImage()->getObjects(objs);
    for (auto obj : objs) {
        if (path->compare(obj->pathName()) == 0) {
            Dyninst::SymtabAPI::convert(obj)->removeLibraryDependency(HELPER_LIB);
            return;
        }
    }
    std::cerr << "Failed to remove duplicate dep on helper lib." << std::endl;
}

bool DynProf::writeOutput() {
    std::string out_file = executable + "_dynprof";
    bool status = static_cast<BPatch_binaryEdit*>(app)->writeFile(out_file.c_str());
    if (status) {
        std::cerr << "Modified binary written to: " << out_file << std::endl;
    } else {
        std::cerr << "Failed to write modified binary to: " << out_file << std::endl;
    }
    return !status;
}

double DynProf::elapsed_time(struct timespec* before, struct timespec* after) {
    std::chrono::nanoseconds before_c =
        std::chrono::seconds(before->tv_sec) + std::chrono::nanoseconds(before->tv_nsec);
    std::chrono::nanoseconds after_c =
        std::chrono::seconds(after->tv_sec) + std::chrono::nanoseconds(after->tv_nsec);
    std::chrono::nanoseconds elapsed = after_c - before_c;
    return elapsed.count();
}

void DynProf::shutdown() {
    if (app && app->getType() == processType::TRADITIONAL_PROCESS) {
        static_cast<BPatch_process*>(app)->terminateExecution();
    }
    exit(1);
}
