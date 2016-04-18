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

#include "libdynprof.h"

FuncMap& func_map() {
    static FuncMap* func_map = new FuncMap();
    return *func_map;
}

void copy_func_map(FuncMap* input) {
    func_map().insert(input->begin(), input->end());
}


void __dynprof_register_handler() {
    if (atexit(exit_handler)) {
        std::cerr << "Failed to register atexit handler." << std::endl;
        exit(1);
    }
}
void exit_handler() {
    std::cerr << "memes:" << func_map().size() << endl;
    for(auto& func: func_map()) {
      std::cerr << "count:" << func.second->count << std::endl;
    }
}
