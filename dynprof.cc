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

#define DEBUG 1

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

std::ostream& operator<< (std::ostream &out, const FuncInfo& info) {
    out << "children: " << info.children.size();
    return out;
}

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
            // TODO(peter): deal with library functions?
            if (subfunc->isSharedLib()) {
                // cout << "skip:" << subfunc->getName() << endl;
            } else {
#if DEBUG
                std::cout << "Visited subroutine " << subfunc->getName() << std::endl;
#endif
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
        std::cerr << "Found " << funcs->size() << " matches for: " << name << std::endl;
        shutdown();
    }
    return funcs->at(0);
}

void DynProf::hook_functions() {
    BPatch_function* func = get_function(DEFAULT_ENTRY_POINT);
#if DEBUG
    std::cerr << "Found entry point " << func->getName() << std::endl;
#endif
    enum_subroutines(func);
    registerCleanupSnippet();
}

bool DynProf::createBeforeSnippet(BPatch_function* func) {
    std::unique_ptr<std::vector<BPatch_point*>> entry_points(func->findPoint(BPatch_entry));
    if (!entry_points || entry_points->size() == 0) {
        std::cerr << "Could not find entry point for " << func->getName() << std::endl;
        return false;
    }

    std::vector<BPatch_snippet*> entry_vec;
#if DEBUG
    std::vector<BPatch_snippet*> entry_args;
    entry_args.push_back(new BPatch_constExpr("Entering %s\n"));
    entry_args.push_back(new BPatch_constExpr(func->getName().c_str()));
    entry_vec.push_back(new BPatch_funcCallExpr(*printf_func, entry_args));
#endif

    entry_vec.push_back(new BPatch_arithExpr(
        BPatch_assign, *func_map[func]->count,
        BPatch_arithExpr(BPatch_plus, *func_map[func]->count, BPatch_constExpr(1))));

    std::vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(new BPatch_arithExpr(BPatch_addr, *func_map[func]->before));
    entry_vec.push_back(new BPatch_funcCallExpr(*clock_func, clock_args));

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

    std::vector<BPatch_snippet*> exit_vec;
#if DEBUG
    std::vector<BPatch_snippet*> exit_args;
    exit_args.push_back(new BPatch_constExpr("Exiting %s\n"));
    exit_args.push_back(new BPatch_constExpr(func->getName().c_str()));
    exit_vec.push_back(new BPatch_funcCallExpr(*printf_func, exit_args));
#endif

    std::vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(new BPatch_arithExpr(BPatch_addr, *func_map[func]->after));
    exit_vec.push_back(new BPatch_funcCallExpr(*clock_func, clock_args));

    BPatch_sequence exit_seq(exit_vec);
    for (auto exit_point : *exit_points) {
        app->insertSnippet(exit_seq, *exit_point, BPatch_callAfter);
    }
    return true;
}

void DynProf::registerCleanupSnippet() {
    std::vector<BPatch_function*> exit_funcs;
    app->getImage()->findFunction("__dynprof_register_handler", exit_funcs);
    if (exit_funcs.size() != 1) {
        std::cerr << "Could not find handler registration function." << std::endl;
        shutdown();
    }
    BPatch_funcCallExpr atexit_reg(*exit_funcs[0], {new BPatch_constExpr(func_map.size())});

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
    //BPatch_function* elapsed_func = get_function("elapsed_time", true);
    // FIXME: figure out when to free this.
    //BPatch_variableExpr* elapsed_count = app->malloc(*app->getImage()->findType("double"));
    BPatch_variableExpr* funcs = app->getImage()->findVariable("funcs");

    std::vector<BPatch_snippet*> snippets;
    for (auto it = func_map.begin(); it != func_map.end(); ++it) {
        long idx = std::distance(it, func_map.begin());
        if (it->first->getName() == DEFAULT_ENTRY_POINT) {
            continue;
        }
        std::vector<BPatch_snippet*> elapsed_args;
        BPatch_snippet lib_func = BPatch_arithExpr(BPatch_ref, *funcs, BPatch_constExpr(idx));
        elapsed_args.push_back(new BPatch_arithExpr(BPatch_addr, *it->second->before));
        elapsed_args.push_back(new BPatch_arithExpr(BPatch_addr, *it->second->after));
        //elapsed_args.push_back(new BPatch_arithExpr(BPatch_addr, *elapsed_count));

        // TODO: use DynC?
        std::vector<BPatch_snippet*> exit_args;
        exit_args.push_back(new BPatch_constExpr("FOO\tFOO\t\t%f\t%d\t%s\n"));
        //FIXME: exit_args.push_back(new BPatch_arithExpr(BPatch_deref, *elapsed_count));
        exit_args.push_back(it->second->count);
        exit_args.push_back(new BPatch_constExpr(it->first->getName().c_str()));

        // Only print the summary if the function has been called.
        //BPatch_funcCallExpr *elapsed_snip = new BPatch_funcCallExpr(*elapsed_func, elapsed_args);
        // FIXME: should we really be using printf here?
        BPatch_funcCallExpr *func_snip = new BPatch_funcCallExpr(*printf_func, exit_args);
        BPatch_boolExpr count_expr(BPatch_ne, *it->second->count, BPatch_constExpr(0));
        snippets.push_back(new BPatch_ifExpr(count_expr, BPatch_sequence({/*elapsed_snip,*/ func_snip})));
    }
    BPatch_sequence exit_snippet(snippets);

    /* FIXME: don't wanna re-write libdynprof.so
    BPatch_function* handler_func = get_function("exit_handler");
    BPatch_module* lib_mod = handler_func->getModule();
    std::unique_ptr<std::vector<BPatch_point*>> handler_entry_points(
        handler_func->findPoint(BPatch_exit));
    if (!handler_entry_points || handler_entry_points->size() == 0) {
        std::cerr << "Could not find entry point for " << handler_func->getName() << std::endl;
        shutdown();
    }
    if (!app->insertSnippet(exit_snippet, *handler_entry_points->at(0), BPatch_callAfter)) {
        std::cerr << "Could not insert summary snippet." << std::endl;
        shutdown();
    }
    */
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
        std::cerr << "Failed to create struct timespec." << std::endl;
        shutdown();
    }
}

void DynProf::find_funcs() {
    app->loadLibrary(resolve_path(HELPER_LIB).c_str());
    clock_func = get_function("clock_gettime");
#if 1 //FIXME: only enable when DEBUG
    printf_func = get_function("printf", true);
#endif
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

void DynProf::shutdown() {
    if (app && app->getType() == processType::TRADITIONAL_PROCESS) {
        static_cast<BPatch_process*>(app)->terminateExecution();
    }
    exit(1);
}
