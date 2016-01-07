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

#if DYNINST_MAJOR < 9
#error "Please use Dyninst 9.0 or newer"
#endif

#include <unordered_map>

#define DEFAULT_ENTRY_POINT "main"

class FuncInfo {
   public:
    FuncInfo(BPatch_variableExpr* _count) : children(), count(_count) {}
    FuncInfo(const FuncInfo&) = delete;
    FuncInfo& operator=(const FuncInfo&) = delete;
    void addChild(BPatch_function*);
    BPatch_variableExpr getCount();

   private:
    vector<BPatch_function*> children;
    // FIXME: do const stuff
    BPatch_variableExpr* count;
};

class DynProf {
   public:
    DynProf(string _path, const char** _params)
        : path(_path), params(_params), app(), bpatch(), func_map() {}
    DynProf(const DynProf&) = delete;
    DynProf& operator=(const DynProf&) = delete;
    void start();
    int waitForExit();
    void printCallCounts();

   private:
    string path;
    const char** params;
    BPatch_process* app;
    BPatch bpatch;
    unordered_map<BPatch_function*, FuncInfo*> func_map;
    unique_ptr<vector<BPatch_function*>> get_entry_point();
    void hook_functions();
    void enum_subroutines(BPatch_function* func);
    void createSnippets(BPatch_function* func);
    void recordFunc(BPatch_function* func);
};

const char** get_params(vector<string> args);
unique_ptr<string> get_path(string exe);
