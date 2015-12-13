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

#define DEFAULT_ENTRY_POINT "main"

unique_ptr<string> get_path(char* exe);

const char** get_params(int argc, char* argv[]);

unique_ptr<vector<BPatch_function*>> get_entry_points(BPatch_process* proc);

void enum_subroutines(BPatch_function func);

void hook_functions(BPatch_process* proc);

void code_discover(BPatch_Vector<BPatch_function*>& newFuncs,
                   BPatch_Vector<BPatch_function*>& modFuncs);
