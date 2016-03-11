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

#ifndef DYNPROF_H
#define DYNPROF_H

// PCH
#include "dyninst.h"

#if DYNINST_MAJOR < 9
#error "Please use Dyninst 9.0 or newer"
#endif

#define DEFAULT_ENTRY_POINT "main"
#define HELPER_LIB "libdynprof.so"

#include <chrono>
#include <unordered_map>

std::string resolve_path(std::string file);

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
    std::chrono::nanoseconds elapsed;

   private:
    std::vector<BPatch_function*> children;
};

class DynProf {
   public:
    DynProf(std::unique_ptr<std::string> _path, const char** _params)
        : path(std::move(_path)),
          params(_params),
          executable(),
          app(nullptr),
          timespec_struct(nullptr),
          clock_func(nullptr),
          printf_func(nullptr),
          bpatch(),
          func_map() {
        size_t offset = path->rfind("/");
        executable = path->substr(offset + 1);
    }
    DynProf(const DynProf&) = delete;
    DynProf& operator=(const DynProf&) = delete;
    void start();
    void setupBinary();
    int waitForExit();
    bool writeOutput();

   private:
    std::unique_ptr<std::string> path;
    const char** params;
    std::string executable;
    BPatch_addressSpace* app;
    BPatch_type* timespec_struct;
    BPatch_function* clock_func;
    BPatch_function* printf_func;
    BPatch bpatch;
    std::unordered_map<BPatch_function*, FuncInfo*> func_map;
    BPatch_function* get_function(std::string name, bool uninstrumentable = false);
    void hook_functions();
    void create_structs();
    void find_funcs();
    void update_needed();
    void doSetup();
    void enum_subroutines(BPatch_function* func);
    bool createBeforeSnippet(BPatch_function* func);
    bool createAfterSnippet(BPatch_function* func);
    void registerCleanupSnippet();
    void createSnippets(BPatch_function* func);
    void recordFunc(BPatch_function* func);
    [[noreturn]] void shutdown();
    double elapsed_time(struct timespec* before, struct timespec* after);
};

#endif
