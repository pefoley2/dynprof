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

using Dyninst::Architecture;
using Dyninst::Stackwalker::Walker;

std::string resolve_path(std::string file) {
    char resolved_path[PATH_MAX];
    if (realpath(file.c_str(), resolved_path)) {
        if (access(resolved_path, F_OK) == 0) {
            return std::string(resolved_path);
        }
    }
    return file;
}

void DynProf::recordFunc(BPatch_function* func) {
    BPatch_type* int_type = app->getImage()->findType("int");
    assert(int_type);
    BPatch_type* bool_type = app->getImage()->findType("int");
    assert(bool_type);
    BPatch_variableExpr* id = app->malloc(*int_type);
    BPatch_variableExpr* type = app->malloc(*bool_type);
    BPatch_variableExpr* before = app->malloc(*timespec_struct);
    BPatch_variableExpr* after = app->malloc(*timespec_struct);
    func_map.insert(std::make_pair(func, new FuncInfo(id, type, before, after)));
}

void DynProf::save_child(BPatch_function* parent, BPatch_point* child) {
    // TODO(peter): implement
    std::cerr << "FOO:" << parent->getName() << ":" << child->getCalledFunction()->getName()
              << std::endl;
}

void DynProf::enum_subroutines(BPatch_function* func) {
    // Already visited.
    if (func_map.count(func)) {
        return;
    }
    recordFunc(func);
    // Register entry/exit snippets.
    if (!createBeforeSnippet(func) || !createAfterSnippet(func)) {
        return;
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
                save_child(func, subroutine);
                enum_subroutines(subfunc);
            }
        }
    }
}

BPatch_function* DynProf::get_function(const std::string& name, bool uninstrumentable) {
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
    app->beginInsertionSet();

    enum_subroutines(func);
    registerCleanupSnippet();

    if (!app->finalizeInsertionSet(true)) {
        std::cerr << "Failed to insert snippets." << std::endl;
    }
}

// Poor-man's serialization: write(fd, foo, sizeof(struct foo)
BPatch_funcCallExpr* DynProf::writeSnippet(BPatch_snippet* ptr, size_t len) {
    auto name_args = new std::vector<BPatch_snippet*>;
    name_args->push_back(output_var);
    name_args->push_back(ptr);
    name_args->push_back(new BPatch_constExpr(len));
    return new BPatch_funcCallExpr(*write_func, *name_args);
}

bool DynProf::createBeforeSnippet(BPatch_function* func) {
    std::string name = func->getName();
    std::unique_ptr<std::vector<BPatch_point*>> entry_points(func->findPoint(BPatch_entry));
    if (!entry_points || entry_points->empty()) {
        std::cerr << "Could not find entry point for " << name << std::endl;
        return false;
    }

    std::vector<BPatch_snippet*> entry_vec;
#if DEBUG
    std::vector<BPatch_snippet*> entry_args;
    entry_args.push_back(new BPatch_constExpr("Entering %s\n"));
    entry_args.push_back(new BPatch_constExpr(name.c_str()));
    entry_vec.push_back(new BPatch_funcCallExpr(*printf_func, entry_args));
#endif

    // The snippets are sorted in reverse order here.
    /*
    std::vector<BPatch_snippet*> parent_args;
    //parent_args.push_back(new BPatch_registerExpr(MachRegister::getReturnAddress(arch)));
    parent_args.push_back(new BPatch_registerExpr(MachRegister::getStackPointer(arch)));
    parent_args.push_back(new BPatch_registerExpr(MachRegister::getFramePointer(arch)));
    entry_vec.push_back(new BPatch_funcCallExpr(*parent_func, parent_args));
    FIXME: https://github.com/dyninst/dyninst/issues/40#issuecomment-219115905
    */

    // Time at start of function
    entry_vec.push_back(writeSnippet(func_map[func]->before, sizeof(struct timespec)));

    // Calculate time at start of function
    std::vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(new BPatch_arithExpr(BPatch_addr, *func_map[func]->before));
    entry_vec.push_back(new BPatch_funcCallExpr(*clock_func, clock_args));

    // Unique id for this call
    entry_vec.push_back(
        writeSnippet(new BPatch_arithExpr(BPatch_addr, *func_map[func]->id), sizeof(int)));

    // Next call is going to be an exit snippet.
    entry_vec.push_back(
        new BPatch_arithExpr(BPatch_assign, *func_map[func]->type, BPatch_constExpr(true)));

    // Type of this call
    entry_vec.push_back(
        writeSnippet(new BPatch_arithExpr(BPatch_addr, *func_map[func]->type), sizeof(bool)));

    // Function name (with trailing null)
    entry_vec.push_back(writeSnippet(new BPatch_constExpr(name.c_str()), name.size() + 1));

    for (auto entry_point : *entry_points) {
        for (auto entry_snip : entry_vec) {
            app->insertSnippet(*entry_snip, *entry_point, BPatch_callBefore);
        }
    }
    return true;
}

bool DynProf::createAfterSnippet(BPatch_function* func) {
    std::string name = func->getName();
    std::unique_ptr<std::vector<BPatch_point*>> exit_points(func->findPoint(BPatch_exit));
    if (!exit_points || exit_points->empty()) {
        std::cerr << "Could not find exit point for " << name << std::endl;
        return false;
    }

    std::vector<BPatch_snippet*> exit_vec;
#if DEBUG
    std::vector<BPatch_snippet*> exit_args;
    exit_args.push_back(new BPatch_constExpr("Exiting %s\n"));
    exit_args.push_back(new BPatch_constExpr(func->getName().c_str()));
    exit_vec.push_back(new BPatch_funcCallExpr(*printf_func, exit_args));
#endif

    // The snippets are sorted in reverse order here.

    // Time at end of function
    exit_vec.push_back(writeSnippet(func_map[func]->after, sizeof(struct timespec)));

    // Calculate time at start of function
    std::vector<BPatch_snippet*> clock_args;
    clock_args.push_back(new BPatch_constExpr(CLOCK_MONOTONIC));
    clock_args.push_back(new BPatch_arithExpr(BPatch_addr, *func_map[func]->after));
    exit_vec.push_back(new BPatch_funcCallExpr(*clock_func, clock_args));

    // Increment id for next call
    exit_vec.push_back(new BPatch_arithExpr(
        BPatch_assign, *func_map[func]->id,
        BPatch_arithExpr(BPatch_plus, *func_map[func]->id, BPatch_constExpr(1))));

    // Unique id for this call
    exit_vec.push_back(
        writeSnippet(new BPatch_arithExpr(BPatch_addr, *func_map[func]->id), sizeof(int)));

    // Next call (if any) is going to be an entry snippet.
    exit_vec.push_back(
        new BPatch_arithExpr(BPatch_assign, *func_map[func]->type, BPatch_constExpr(false)));

    // type of this call
    exit_vec.push_back(
        writeSnippet(new BPatch_arithExpr(BPatch_addr, *func_map[func]->type), sizeof(bool)));

    // Function name (with trailing null)
    exit_vec.push_back(writeSnippet(new BPatch_constExpr(name.c_str()), name.size() + 1));

    // Functions can have multiple exit points.
    for (auto exit_point : *exit_points) {
        for (auto exit_snip : exit_vec) {
            app->insertSnippet(*exit_snip, *exit_point, BPatch_callAfter);
        }
    }
    return true;
}

void DynProf::registerCleanupSnippet() {
    BPatch_function* exit_func = get_function("__dynprof_register_handler");
    BPatch_funcCallExpr atexit_reg(*exit_func, {});

    BPatch_function* func = get_function(DEFAULT_ENTRY_POINT);
    std::unique_ptr<std::vector<BPatch_point*>> entry_points(func->findPoint(BPatch_entry));
    if (!entry_points || entry_points->size() != 1) {
        std::cerr << "Could not find exactly one entry point for " << func->getName() << std::endl;
        shutdown();
    }
    // Need to zero-out the BPatch_variableExprs for main()
    BPatch_arithExpr id_snip(BPatch_assign, *func_map[func]->id, BPatch_constExpr(0));
    app->insertSnippet(id_snip, *entry_points->at(0), BPatch_callBefore);
    // FIXME: needed?
    // BPatch_arithExpr before_snip(BPatch_assign, *func_map[func]->before, BPatch_constExpr(0));
    // BPatch_arithExpr after_snip(BPatch_assign, *func_map[func]->after, BPatch_constExpr(0));
    // app->insertSnippet(before_snip, *entry_points->at(0), BPatch_callBefore);
    // app->insertSnippet(after_snip, *entry_points->at(0), BPatch_callBefore);

    if (!app->insertSnippet(atexit_reg, *entry_points->at(0), BPatch_callBefore)) {
        std::cerr << "Could not insert atexit snippet." << std::endl;
        shutdown();
    }
}

void DynProf::create_structs() {
    const char* sec_name = "tv_sec";
    const char* nsec_name = "tv_nsec";
    std::vector<char*> time_field_names{const_cast<char*>(sec_name), const_cast<char*>(nsec_name)};
    std::vector<BPatch_type*> time_field_types{
        app->getImage()->findType("long"),  // time_t is ultimately a typedef to long
        app->getImage()->findType("long")};
    timespec_struct = bpatch.createStruct("timespec", time_field_names, time_field_types);
    if (!timespec_struct) {
        std::cerr << "Failed to create struct timespec." << std::endl;
        shutdown();
    }
}

void DynProf::find_funcs() {
    app->loadLibrary(resolve_path(HELPER_LIB).c_str());
    output_var = app->getImage()->findVariable("__dynprof_output_fd");
    if (!output_var) {
        std::cerr << "Could not find output var." << std::endl;
        shutdown();
    }
    clock_func = get_function("clock_gettime");
    parent_func = get_function("__dynprof_get_parent");
    std::unique_ptr<Walker> w(Walker::newWalker());
    arch = w->getProcessState()->getArchitecture();
    if (arch != Architecture::Arch_x86_64) {
        std::cerr << "Only x86_64 currently supported." << std::endl;
        shutdown();
    }

    std::unique_ptr<std::vector<BPatch_function*>> funcs(new std::vector<BPatch_function*>);
    app->getImage()->findFunction("write", *funcs);
    // glibc has two different internal definitions of write()
    // Remove the one that causes an undefined reference.
    if (funcs->size() != 1) {
        for (size_t i = 0; i < funcs->size(); i++) {
            if (funcs->at(i)->getName() == "__GI___write") {
                funcs->erase(funcs->begin() + static_cast<int64_t>(i));
                i--;
            }
        }
    }
    if (funcs->size() != 1) {
        std::cerr << "Found " << funcs->size() << " matches for: write" << std::endl;
        shutdown();
    }
    write_func = funcs->at(0);

#if DEBUG
    printf_func = get_function("printf", true);
#endif
}

void DynProf::start() {
    std::cerr << "Preparing to profile " << *path << std::endl;
    app = bpatch.processCreate(path->c_str(), params);
    if (dynamic_cast<BPatch_process*>(app)->isMultithreadCapable()) {
        // TODO(peter): handle entry points other than main().
        // app->getThreads()
        std::cerr << "Multithreading is not yet handled." << std::endl;
        shutdown();
    }
    doSetup();
    std::cerr << "Resuming execution" << std::endl;
    dynamic_cast<BPatch_process*>(app)->continueExecution();
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
    while (!dynamic_cast<BPatch_process*>(app)->isTerminated()) {
        bpatch.waitForStatusChange();
    }
    int status = dynamic_cast<BPatch_process*>(app)->getExitCode();
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
    bool status = dynamic_cast<BPatch_binaryEdit*>(app)->writeFile(out_file.c_str());
    if (status) {
        std::cerr << "Modified binary written to: " << out_file << std::endl;
    } else {
        std::cerr << "Failed to write modified binary to: " << out_file << std::endl;
    }
    return !status;
}

void DynProf::shutdown() {
    if (app && app->getType() == processType::TRADITIONAL_PROCESS) {
        dynamic_cast<BPatch_process*>(app)->terminateExecution();
    }
    exit(1);
}
