/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeImport.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/parser/parser.hpp"
#include <algorithm>
#include <fstream>
#include <chrono>
#include "../../include/compiler.hpp"

#ifdef _WIN32
   #include <io.h> 
   #define access    _access_s
#else
   #include <unistd.h>
#endif

#ifndef __has_include
  static_assert(false, "__has_include not supported");
#else
#if __cplusplus >= 201703L && __has_include(<filesystem>)
#include <filesystem>
    namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#elif __has_include(<boost/filesystem.hpp>)
#include <boost/filesystem.hpp>
    namespace fs = boost::filesystem;
#  endif
#endif

NodeImport::NodeImport(std::string file, std::vector<std::string> functions, int loc) {
    this->file = file;
    this->functions = functions;
    this->loc = loc;
}

Type* NodeImport::getType() {return typeVoid;}
Node* NodeImport::comptime() {return this;}
Node* NodeImport::copy() {return new NodeImport(this->file, this->functions, this->loc);}
void NodeImport::check() {this->isChecked = true;}

RaveValue NodeImport::generate() {
    if(std::find(AST::importedFiles.begin(), AST::importedFiles.end(), this->file) != AST::importedFiles.end() || this->file == generator->file) return {};

    if(this->file.find("/.rave") != std::string::npos) {
        std::string dirPath = this->file.substr(0, this->file.size() - 5);
        for(const auto& entry : fs::directory_iterator(dirPath)) {
            if(entry.path().extension() == ".rave") {
                NodeImport* imp = new NodeImport({}, functions, loc);
                imp->file = entry.path().string();
                imp->generate();
                delete imp;
            }
        }
        return {};
    }

    if(AST::parsed.find(this->file) == AST::parsed.end()) {
        if(!fs::exists(this->file)) {
            generator->error("file '" + this->file + "' does not exist!", this->loc);
            return {};
        }

        std::ifstream fContent(this->file);
        std::string content((std::istreambuf_iterator<char>(fContent)), std::istreambuf_iterator<char>());

        content = "alias __RAVE_IMPORTED_FROM = \"" + generator->file + "\"; " + content;

        auto start = std::chrono::steady_clock::now();
        Lexer lexer(content, 1);
        auto end = std::chrono::steady_clock::now();
        Compiler::lexTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        start = end;
        Parser parser = Parser(lexer.tokens, this->file);
        parser.parseAll();
        end = std::chrono::steady_clock::now();
        Compiler::parseTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        AST::parsed[this->file] = parser.nodes;
    }

    std::vector<Node*> buffer;
    for(const auto& node : AST::parsed[this->file]) buffer.push_back(node->copy());

    if(instanceof<NodeVar>(buffer[0])) {
        auto* nodeVar = static_cast<NodeVar*>(buffer[0]);
        if(nodeVar->name == "__RAVE_IMPORTED_FROM") nodeVar->value = new NodeString(generator->file, false);
    }

    for(auto* node : buffer) {
        if(instanceof<NodeFunc>(node)) {
            auto* nodeFunc = static_cast<NodeFunc*>(node);
            if(!nodeFunc->isPrivate) node->check();
        }
        else if(instanceof<NodeNamespace>(node)) {
            auto* nodeNamespace = static_cast<NodeNamespace*>(node);
            nodeNamespace->hidePrivated = true;
            nodeNamespace->isImported = true;
            nodeNamespace->check();
        }
        else node->check();
    }

    std::string oldFile = generator->file;
    generator->file = this->file;
    auto start = std::chrono::steady_clock::now();

    for(auto* node : buffer) {
        if(instanceof<NodeFunc>(node)) {
            auto* nodeFunc = static_cast<NodeFunc*>(node);
            if(nodeFunc->isPrivate) continue;
            nodeFunc->isExtern = true;
        }
        else if(instanceof<NodeVar>(node)) {
            auto* nodeVar = static_cast<NodeVar*>(node);
            if(nodeVar->isPrivate) continue;
            nodeVar->isExtern = true;
        }
        else if(instanceof<NodeStruct>(node)) {
            auto* nodeStruct = static_cast<NodeStruct*>(node);
            nodeStruct->isImported = true;
        }
        else if(instanceof<NodeBuiltin>(node)) {
            auto* nodeBuiltin = static_cast<NodeBuiltin*>(node);
            nodeBuiltin->isImport = true;
        }
        node->generate();
    }

    auto end = std::chrono::steady_clock::now();
    Compiler::genTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    generator->file = oldFile;
    AST::importedFiles.push_back(this->file);
    return {};
}

NodeImports::NodeImports(std::vector<NodeImport*> imports, int loc) {
    this->imports = std::vector<NodeImport*>(imports);
    this->loc = loc;
}

Node* NodeImports::comptime() {return nullptr;}
void NodeImports::check() {this->isChecked = true;}
Type* NodeImports::getType() {return typeVoid;}

Node* NodeImports::copy() {
    std::vector<NodeImport*> buffer;
    for(int i=0; i<this->imports.size(); i++) buffer.push_back((NodeImport*)(this->imports[i]->copy()));
    return new NodeImports(buffer, this->loc);
}

RaveValue NodeImports::generate() {
    for(int i=0; i<this->imports.size(); i++) {
        if(this->imports[i] != nullptr) this->imports[i]->generate();
    }
    return {};
}