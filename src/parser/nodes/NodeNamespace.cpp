/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeAliasType.hpp"
#include "../../include/utils.hpp"
#include <string>

NodeNamespace::NodeNamespace(std::string name, std::vector<Node*> nodes, long loc) {
    this->loc = loc;
    this->nodes = std::vector<Node*>(nodes);
    this->names = std::vector<std::string>();
    this->names.push_back(name);
}

NodeNamespace::NodeNamespace(std::vector<std::string> names, std::vector<Node*> nodes, long loc) {
    this->loc = loc;
    this->nodes = std::vector<Node*>(nodes);
    this->names = std::vector<std::string>();
    for(int i=0; i<names.size(); i++) this->names.push_back(names[i]);
}

Node* NodeNamespace::copy() {
    std::vector<Node*> cNodes;
    for(int i=0; i<this->nodes.size(); i++) cNodes.push_back(this->nodes[i]);
    return new NodeNamespace(std::vector<std::string>(this->names), cNodes, this->loc);
}

Type* NodeNamespace::getType() {return new TypeVoid();}
Node* NodeNamespace::comptime() {return nullptr;}

void NodeNamespace::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    //std::cout << "\tChecking " << names[0] << std::endl;

    if(!oldCheck) for(int i=0; i<this->nodes.size(); i++) {
        if(this->nodes[i] == nullptr) continue;
        if(instanceof<NodeFunc>(this->nodes[i])) {
            NodeFunc* nfunc = (NodeFunc*)this->nodes[i];
            if(hidePrivated && nfunc->isPrivate) continue;
            if(this->isImported && nfunc->isChecked) {
                nfunc->isChecked = false;
                if(nfunc->namespacesNames.size() > 0 ) {
                    nfunc->name = nfunc->origName;
                    nfunc->namespacesNames.clear();
                }
            }
            for(int i=0; i<this->names.size(); i++) nfunc->namespacesNames.insert(nfunc->namespacesNames.begin(), this->names[i]);
            this->nodes[i]->check();
        }
        else if(instanceof<NodeNamespace>(this->nodes[i])) {
            NodeNamespace* nnamespace = (NodeNamespace*)this->nodes[i];
            if(this->isImported && nnamespace->isChecked) {
                nnamespace->isChecked = false;
                //nnamespace->names.erase(nnamespace->names.begin());
            }
            else for(int i=0; i<this->names.size(); i++) nnamespace->names.insert(nnamespace->names.begin()+i, this->names[i]);
            nnamespace->isImported = (nnamespace->isImported || this->isImported);
            this->nodes[i]->check();
        }
        else if(instanceof<NodeVar>(this->nodes[i])) {
            NodeVar* nvar = (NodeVar*)this->nodes[i];
            if(hidePrivated && nvar->isPrivate) continue;
            if(this->isImported && nvar->isChecked) {
                nvar->isChecked = false;
                nvar->name = nvar->origName;
                nvar->namespacesNames.clear();
            }
            for(int i=0; i<this->names.size(); i++) nvar->namespacesNames.insert(nvar->namespacesNames.begin(), this->names[i]);
            this->nodes[i]->check();
        }
        else if(instanceof<NodeStruct>(this->nodes[i])) {
            NodeStruct* structure = ((NodeStruct*)this->nodes[i]);
            if(this->isImported && structure->isChecked) {
                structure->isChecked = false;
                structure->name = structure->origname;
                structure->namespacesNames.clear();
            }
            for(int i=0; i<this->names.size(); i++) structure->namespacesNames.insert(structure->namespacesNames.begin(), this->names[i]);
            this->nodes[i]->check();
        }
        else if(instanceof<NodeAliasType>(this->nodes[i])) {
            NodeAliasType* naliastype = (NodeAliasType*)this->nodes[i];
            for(int i=0; i<this->names.size(); i++) naliastype->namespacesNames.insert(naliastype->namespacesNames.begin(), this->names[i]);
            this->nodes[i]->check();
        }
    }
}

LLVMValueRef NodeNamespace::generate() {
    for(int i=0; i<this->nodes.size(); i++) {
        if(this->nodes[i] == nullptr) continue;
        if(instanceof<NodeFunc>(this->nodes[i])) {
            NodeFunc* nfunc = (NodeFunc*)this->nodes[i];
            if(hidePrivated && (nfunc->isPrivate)) continue;
            if(!this->nodes[i]->isChecked) {
                for(int i=0; i<this->names.size(); i++) nfunc->namespacesNames.insert(nfunc->namespacesNames.begin(), this->names[i]);
                this->nodes[i]->check();
            }
            nfunc->isExtern = (nfunc->isExtern || this->isImported);
            nfunc->generate();
        }
        else if(instanceof<NodeNamespace>(this->nodes[i])) {
            NodeNamespace* nnamespace = (NodeNamespace*)this->nodes[i];
            if(!this->nodes[i]->isChecked) {
                for(int i=0; i<this->names.size(); i++) nnamespace->names.insert(nnamespace->names.begin(), this->names[i]);
                this->nodes[i]->check();
            }
            nnamespace->isImported = (nnamespace->isImported || this->isImported);
            nnamespace->generate();
        }
        else if(instanceof<NodeVar>(this->nodes[i])) {
            NodeVar* nvar = (NodeVar*)this->nodes[i];
            if(hidePrivated && nvar->isPrivate) continue;
            if(!this->nodes[i]->isChecked) {
                for(int i=0; i<this->names.size(); i++) nvar->namespacesNames.insert(nvar->namespacesNames.begin(), this->names[i]);
                this->nodes[i]->check();
            }
            nvar->isExtern = (nvar->isExtern || this->isImported);
            nvar->generate();
        }
        else if(instanceof<NodeStruct>(this->nodes[i])) {
            NodeStruct* nstruct = (NodeStruct*)this->nodes[i];
            if(!this->nodes[i]->isChecked) {
                for(int i=0; i<this->names.size(); i++) nstruct->namespacesNames.insert(nstruct->namespacesNames.begin(), this->names[i]);
                this->nodes[i]->check();
            }
            nstruct->isImported = (nstruct->isImported || this->isImported);
            nstruct->generate();
        }
        else if(instanceof<NodeAliasType>(this->nodes[i])) {
            if(!this->nodes[i]->isChecked) {
                for(int i=0; i<this->names.size(); i++) ((NodeAliasType*)nodes[i])->namespacesNames.push_back(this->names[i]);
                this->nodes[i]->check();
            }
            this->nodes[i]->generate();
        }
    }
    return nullptr;
}