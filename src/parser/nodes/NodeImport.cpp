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

#ifdef _WIN32
   #include <io.h> 
   #define access    _access_s
#else
   #include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
std::vector<std::string> filesFromDirectory(std::string path)
{
    std::vector<std::string> files;

    // check directory exists
    char fullpath[MAX_PATH];
    GetFullPathName(path.c_str(), MAX_PATH, fullpath, 0);
    std::string fp(fullpath);
    if (GetFileAttributes(fp.c_str()) != FILE_ATTRIBUTE_DIRECTORY)
        return files;

    // get file names
    WIN32_FIND_DATA findfiledata;
    HANDLE hFind = FindFirstFile((LPCSTR)(fp + "\\*").c_str(), &findfiledata);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do 
        {
            files.push_back(findfiledata.cFileName);
        } 
        while (FindNextFile(hFind, &findfiledata));
        FindClose(hFind);
    }

    // delete current and parent directories
    files.erase(std::find(files.begin(), files.end(), "."));
    files.erase(std::find(files.begin(), files.end(), ".."));

    // sort in alphabetical order
    std::sort(files.begin(), files.end());

    return files;
}
#else
#include <dirent.h>
std::vector<std::string> filesFromDirectory(std::string directory)
{
    std::vector<std::string> files;

    // open directory
    DIR *dir;
    dir = opendir(directory.c_str());
    if (dir == NULL)
        return files;

    // get file names
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
        files.push_back(ent->d_name);
    closedir(dir);

    // delete current and parent directories
    files.erase(std::find(files.begin(), files.end(), "."));
    files.erase(std::find(files.begin(), files.end(), ".."));

    // sort in alphabetical order
    std::sort(files.begin(), files.end());

    return files;
}
#endif

NodeImport::NodeImport(std::string file, std::vector<std::string> functions, long loc) {
    this->file = file;
    this->functions = functions;
    this->loc = loc;
}

Type* NodeImport::getType() {return new TypeVoid();}
Node* NodeImport::comptime() {return this;}
Node* NodeImport::copy() {return new NodeImport(this->file, this->functions, this->loc);}
void NodeImport::check() {this->isChecked = true;}

LLVMValueRef NodeImport::generate() {
    if(std::count(AST::importedFiles.begin(), AST::importedFiles.end(), this->file) > 0 || this->file == generator->file) return nullptr;
    std::vector<Node*> buffer;

    if(this->file.find("/.rave") == this->file.size()-6) {
        NodeImport* imp = new NodeImport({}, functions, loc);
        std::vector<std::string> dirFiles = filesFromDirectory(this->file.substr(0, this->file.size()-5));
        for(int i=0; i<dirFiles.size(); i++) {
            if(dirFiles[i].find_last_of(this->file) == this->file.size()-5) imp->file = dirFiles[i];
        }
        imp->generate();
        return nullptr;
    }

    if(AST::parsed.find(this->file) == AST::parsed.end()) {
        if(access(this->file.c_str(), 0) != 0) {
            generator->error("file '"+this->file+"' does not exists!", this->loc);
            return nullptr;
        }
        std::ifstream fContent(this->file);
        std::string content = "";
        char c;
        while(fContent.get(c)) content += c;

        content = "alias __RAVE_IMPORTED_FROM = \""+generator->file+"\"; "+content;
        Lexer* lexer = new Lexer(content, 1);
        Parser* parser = new Parser(lexer->tokens, this->file);
        parser->parseAll();
        AST::parsed[this->file] = std::vector<Node*>({parser->nodes});
        for(int j=0; j<AST::parsed[this->file].size(); j++) buffer.push_back(AST::parsed[this->file][j]->copy());
    }
    else {
        for(int j=0; j<AST::parsed[this->file].size(); j++) buffer.push_back(AST::parsed[this->file][j]->copy());
        if(instanceof<NodeVar>(buffer[0])) {
            if(((NodeVar*)buffer[0])->name == "__RAVE_IMPORTED_FROM") ((NodeVar*)buffer[0])->value = new NodeString(generator->file, false);
        }
    }
    
    for(int j=0; j<buffer.size(); j++) {
        if(instanceof<NodeFunc>(buffer[j])) {
            if(!((NodeFunc*)buffer[j])->isPrivate) buffer[j]->check();
        }
        else if(instanceof<NodeNamespace>(buffer[j])) {
            NodeNamespace* nnamespace = (NodeNamespace*)buffer[j];
            nnamespace->hidePrivated = true;
            nnamespace->isImported = true;
            nnamespace->check();
        }
        else buffer[j]->check();
    }

    std::string oldFile = generator->file;
    generator->file = this->file;
    for(int j=0; j<buffer.size(); j++) {
        if(instanceof<NodeFunc>(buffer[j])) {
            NodeFunc* nfunc = (NodeFunc*)buffer[j];
            if(nfunc->isPrivate) continue;
            nfunc->isExtern = true;
        }
        else if(instanceof<NodeVar>(buffer[j])) {
            NodeVar* nvar = (NodeVar*)buffer[j];
            if(nvar->isPrivate) continue;
            nvar->isExtern = true;
        }
        else if(instanceof<NodeStruct>(buffer[j])) {
            NodeStruct* nstruct = (NodeStruct*)buffer[j];
            nstruct->isImported = true;
        }
        else if(instanceof<NodeBuiltin>(buffer[j])) {
            NodeBuiltin* nbuiltin = (NodeBuiltin*)buffer[j];
            nbuiltin->isImport = true;
        }
        buffer[j]->generate();
    }
    generator->file = oldFile;
    AST::importedFiles.push_back(this->file);
    return nullptr;
}

NodeImports::NodeImports(std::vector<NodeImport*> imports, long loc) {
    this->imports = std::vector<NodeImport*>(imports);
    this->loc = loc;
}

Node* NodeImports::comptime() {return nullptr;}
void NodeImports::check() {this->isChecked = true;}
Type* NodeImports::getType() {return new TypeVoid();}

Node* NodeImports::copy() {
    std::vector<NodeImport*> buffer;
    for(int i=0; i<this->imports.size(); i++) buffer.push_back((NodeImport*)(this->imports[i]->copy()));
    return new NodeImports(buffer, this->loc);
}

LLVMValueRef NodeImports::generate() {
    for(int i=0; i<this->imports.size(); i++) {
        if(this->imports[i] != nullptr) this->imports[i]->generate();
    }
    return nullptr;
}