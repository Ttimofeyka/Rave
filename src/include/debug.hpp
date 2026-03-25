/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <iostream>
#include <string>

namespace Debug {
    // Enable/disable debug output globally
    extern bool enabled;

    // Debug categories for organized output
    enum class Category {
        TypeGen,      // Type generation (LLVMGen::genType, etc.)
        FuncCall,     // Function calls (NodeCall, etc.)
        VarGen,       // Variable generation (NodeVar, etc.)
        Scope,        // Scope management (get, getVar, etc.)
        Template,     // Template instantiation
        CodeGen,      // General code generation
        Parser        // Parsing operations
    };

    // Convert category to string for output
    inline const char* categoryToString(Category cat) {
        switch (cat) {
            case Category::TypeGen:   return "TYPE";
            case Category::FuncCall:  return "CALL";
            case Category::VarGen:    return "VAR";
            case Category::Scope:     return "SCOPE";
            case Category::Template:  return "TMPL";
            case Category::CodeGen:   return "GEN";
            case Category::Parser:    return "PARSE";
            default:                  return "???";
        }
    }

    // Log a debug message with category
    inline void log(Category cat, const std::string& msg) {
        if (enabled) {
            std::cerr << "[\033[36m" << categoryToString(cat) << "\033[0m] " << msg << std::endl;
        }
    }

    // Log with location info
    inline void log(Category cat, const std::string& msg, int line) {
        if (enabled) {
            std::cerr << "[\033[36m" << categoryToString(cat) << "\033[0m] "
                      << msg << " (line " << line << ")" << std::endl;
        }
    }
}

// Conditional debug macro - compiles to nothing when RAVE_DEBUG is not defined
#ifdef RAVE_DEBUG
    #define DEBUG_LOG(cat, msg) Debug::log(cat, msg)
    #define DEBUG_LOG_LOC(cat, msg, line) Debug::log(cat, msg, line)
#else
    #define DEBUG_LOG(cat, msg) ((void)0)
    #define DEBUG_LOG_LOC(cat, msg, line) ((void)0)
#endif