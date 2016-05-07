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
#ifndef DISPLAY_H
#define DISPLAY_H

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

#define HEADER_SIZE 11
#define OUTPUT_VERSION "1"
#define DEFAULT_ENTRY_POINT "main"

struct FuncCall {
    struct timespec time;
};

typedef std::pair<FuncCall, FuncCall> CallPair;
typedef std::unordered_map<int, CallPair> CallMap;
typedef std::unordered_map<std::string, CallMap> FuncMap;

class Output {
   public:
    explicit Output(std::vector<std::string> _args) : args(_args) {}
    int process();

   private:
    int process_file(std::string file);
    bool read_obj(FILE* f, void* ptr, size_t len);
    double elapsed_time(CallPair calls);
    void process_output(FuncMap funcs);
    std::vector<std::string> args;
    static constexpr char expected_header[HEADER_SIZE] = "DYNPROF:" OUTPUT_VERSION "\0";
};

#endif
