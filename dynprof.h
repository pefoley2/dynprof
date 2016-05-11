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

#include <unordered_map>

#if DYNINST_MAJOR < 9
#error "Please use Dyninst 9.0 or newer"
#endif

#define DEBUG 1
#define DEFAULT_ENTRY_POINT "main"
#define HELPER_LIB "libdynprof.so"

std::string resolve_path(std::string file);

class FuncInfo {
   public:
    FuncInfo(BPatch_variableExpr* _id, BPatch_variableExpr* _type, BPatch_variableExpr* _before,
             BPatch_variableExpr* _after)
        : id(_id), type(_type), before(_before), after(_after), children() {}
    FuncInfo(const FuncInfo&) = delete;
    FuncInfo& operator=(const FuncInfo&) = delete;
    void addChild(BPatch_function* func);
    BPatch_variableExpr* const id;
    BPatch_variableExpr* const type;
    BPatch_variableExpr* const before;
    BPatch_variableExpr* const after;

   private:
    std::vector<BPatch_function*> children;
};

typedef std::unordered_map<BPatch_function*, FuncInfo*> FuncMap;

class DynProf {
   public:
    DynProf(std::unique_ptr<std::string> _path, const char** _params)
        : path(std::move(_path)),
          params(_params),
          executable(),
          app(nullptr),
          timespec_struct(nullptr),
          clock_func(nullptr),
          write_func(nullptr),
          parent_func(nullptr),
#if DEBUG
          printf_func(nullptr),
#endif
          output_var(nullptr),
          bpatch(),
          func_map() {
        // TODO(peter): support windows paths?
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
    BPatch_function* write_func;
    BPatch_function* parent_func;
#if DEBUG
    BPatch_function* printf_func;
#endif
    BPatch_variableExpr* output_var;
    BPatch bpatch;
    FuncMap func_map;
    void hook_functions();
    void create_structs();
    void find_funcs();
    void update_needed();
    void doSetup();
    void enum_subroutines(BPatch_function* func);
    void registerCleanupSnippet();
    void recordFunc(BPatch_function* func);
    [[noreturn]] void shutdown();
    bool createBeforeSnippet(BPatch_function* func);
    bool createAfterSnippet(BPatch_function* func);
    BPatch_function* get_function(std::string name, bool uninstrumentable = false);
    BPatch_funcCallExpr* writeSnippet(BPatch_snippet* ptr, size_t len);
};

#endif
