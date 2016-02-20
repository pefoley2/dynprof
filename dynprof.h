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

#ifndef DYNPROF_H
#define DYNPROF_H

// PCH
#include "dyninst.h"

#if DYNINST_MAJOR < 9
#error "Please use Dyninst 9.0 or newer"
#endif

#include <chrono>
#include <unordered_map>

#define DEFAULT_ENTRY_POINT "main"

const char** get_params(vector<string> args);
unique_ptr<string> get_path(string exe);
void ExitCallback(BPatch_thread* proc, BPatch_exitType exit_type);
double elapsed_time(struct timespec* before, struct timespec* after);

class FuncInfo {
   public:
    FuncInfo(BPatch_variableExpr* _count, BPatch_variableExpr* _before, BPatch_variableExpr* _after)
        : count(_count), before(_before), after(_after), elapsed(), children() {}
    FuncInfo(const FuncInfo&) = delete;
    FuncInfo& operator=(const FuncInfo&) = delete;
    void addChild(BPatch_function* func);
    BPatch_variableExpr* const count;
    BPatch_variableExpr* const before;
    BPatch_variableExpr* const after;
    chrono::nanoseconds elapsed;

   private:
    vector<BPatch_function*> children;
};

class DynProf {
   public:
    DynProf(unique_ptr<string> _path, const char** _params)
        : path(std::move(_path)),
          params(_params),
          executable(),
          app(nullptr),
          timespec_struct(nullptr),
          clock_func(nullptr),
          bpatch(),
          func_map(),
          exit_callback(bpatch.registerExitCallback(ExitCallback)) {
        size_t offset = path->rfind("/");
        executable = path->substr(offset + 1);
    }
    DynProf(const DynProf&) = delete;
    DynProf& operator=(const DynProf&) = delete;
    void start();
    void setupBinary();
    int waitForExit();
    bool writeOutput();
    void printCallCounts();
    void printElapsedTime();

   private:
    unique_ptr<string> path;
    const char** params;
    string executable;
    BPatch_addressSpace* app;
    BPatch_type* timespec_struct;
    BPatch_function* clock_func;
    BPatch bpatch;
    unordered_map<BPatch_function*, FuncInfo*> func_map;
    BPatchExitCallback exit_callback;
    unique_ptr<vector<BPatch_function*>> get_entry_point();
    void hook_functions();
    void create_structs();
    void find_funcs();
    void doSetup();
    void enum_subroutines(BPatch_function* func);
    bool createBeforeSnippet(BPatch_function* func);
    bool createAfterSnippet(BPatch_function* func);
    void createSnippets(BPatch_function* func);
    void recordFunc(BPatch_function* func);
};

#endif
