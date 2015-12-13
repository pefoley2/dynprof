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
#include "dynprof.h"

#define DEFAULT_ENTRY_POINT "main"

class DynProf {
   public:
    DynProf(string _path, const char** _params) : path(_path), params(_params) {}
    void start();
    void waitForExit();

   private:
    string path;
    const char** params;
    BPatch_process* app;
    BPatch bpatch;
    unique_ptr<vector<BPatch_function*>> get_entry_point();
    void hook_functions();
    void enum_subroutines(BPatch_function func);
};

const char** get_params(vector<string> args);
unique_ptr<string> get_path(string exe);

/*
void code_discover(BPatch_Vector<BPatch_function*>& newFuncs,
        BPatch_Vector<BPatch_function*>& modFuncs);
*/
#endif
