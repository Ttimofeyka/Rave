/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeBlock.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeAliasType.hpp"
#include "../../include/parser/nodes/NodeComptime.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/utils.hpp"
#include <string>

// TODO: Use for NodeComptime inside NodeNamespace
void addNamespaceNames(Node* node, std::vector<std::string>& names, bool isImported, bool isSetChecked) {
    if(node == nullptr) return;
    if(!isSetChecked && node->isChecked) return;

    if(instanceof<NodeBlock>(node)) {
        NodeBlock* block = (NodeBlock*)node;

        for(size_t i=0; i<block->nodes.size(); i++) {
            addNamespaceNames(block->nodes[i], names, isImported, isSetChecked);
            block->nodes[i]->check();
        }
    }
    else if(instanceof<NodeIf>(node)) {
        NodeIf* _if = (NodeIf*)node;

        if(_if->isStatic) {
            _if->cond = _if->cond->comptime();

            if(((NodeBool*)_if->cond)->value) {
                if(_if->body != nullptr) addNamespaceNames(_if->body, names, isImported, isSetChecked);
            }
            else {
                if(_if->_else != nullptr) addNamespaceNames(_if->_else, names, isImported, isSetChecked);
            }
        }
        else {
            if(_if->body != nullptr) addNamespaceNames(_if->body, names, isImported, isSetChecked);
            if(_if->_else != nullptr) addNamespaceNames(_if->_else, names, isImported, isSetChecked);
        }

        _if->check();
    }
    else if(instanceof<NodeComptime>(node)) {
        addNamespaceNames(((NodeComptime*)node)->node, names, isImported, isSetChecked);
        node->check();
    }
    else if(instanceof<NodeVar>(node)) {
        NodeVar* variable = (NodeVar*)node;

        if(isSetChecked) {
            variable->isChecked = false;
            variable->name = variable->origName;
            variable->namespacesNames.clear();
        }

        variable->origName = variable->name;
        variable->isExtern = variable->isExtern || isImported;

        for(size_t i=0; i<names.size(); i++) variable->namespacesNames.insert(variable->namespacesNames.begin(), names[i]);
        variable->check();
    }
    else if(instanceof<NodeFunc>(node)) {
        NodeFunc* function = (NodeFunc*)node;

        if(isSetChecked) {
            function->isChecked = false;
            function->name = function->origName;
            function->namespacesNames.clear();
        }

        function->origName = function->name;
        function->isExtern = function->isExtern || isImported;

        for(size_t i=0; i<names.size(); i++) function->namespacesNames.insert(function->namespacesNames.begin(), names[i]);
        function->check();
    }
    else if(instanceof<NodeStruct>(node)) {
        NodeStruct* structure = (NodeStruct*)node;

        if(isSetChecked) {
            structure->isChecked = false;
            structure->name = structure->origname;
            structure->namespacesNames.clear();
        }

        structure->origname = structure->name;
        structure->isImported = structure->isImported || isImported;

        for(size_t i=0; i<names.size(); i++) structure->namespacesNames.insert(structure->namespacesNames.begin(), names[i]);
        structure->check();
    }
    else if(instanceof<NodeAliasType>(node)) {
        NodeAliasType* alias = (NodeAliasType*)node;

        if(isSetChecked) {
            alias->isChecked = false;
            alias->name = alias->origName;
            alias->namespacesNames.clear();
        }

        for(size_t i=0; i<names.size(); i++) alias->namespacesNames.insert(alias->namespacesNames.begin(), names[i]);
        alias->check();
    }
    else if(instanceof<NodeNamespace>(node)) {
        NodeNamespace* nNamespace = (NodeNamespace*)node;
        nNamespace->isImported = nNamespace->isImported || isImported;

        if(isSetChecked) nNamespace->isChecked = false;

        for(size_t i=0; i<names.size(); i++) nNamespace->names.insert(nNamespace->names.begin() + i, names[i]);
        nNamespace->check();
    }
}

NodeNamespace::NodeNamespace(std::string name, std::vector<Node*> nodes, int loc) {
    this->loc = loc;
    this->nodes = std::vector<Node*>(nodes);
    this->names = std::vector<std::string>();
    this->names.push_back(name);
}

NodeNamespace::NodeNamespace(std::vector<std::string> names, std::vector<Node*> nodes, int loc) {
    this->loc = loc;
    this->nodes = std::vector<Node*>(nodes);
    this->names = std::vector<std::string>();
    for(size_t i=0; i<names.size(); i++) this->names.push_back(names[i]);
}

NodeNamespace::~NodeNamespace() {
    for(size_t i=0; i<nodes.size(); i++) if(nodes[i] != nullptr) delete nodes[i];
}

Node* NodeNamespace::copy() {
    std::vector<Node*> cNodes;
    for(size_t i=0; i<nodes.size(); i++) cNodes.push_back(nodes[i]);
    return new NodeNamespace(std::vector<std::string>(names), cNodes, loc);
}

Type* NodeNamespace::getType() {return typeVoid;}
Node* NodeNamespace::comptime() {return nullptr;}

void NodeNamespace::check() {
    if(isChecked) return;
    isChecked = true;

    for(size_t i=0; i<nodes.size(); i++) {
        if(nodes[i] == nullptr) continue;

        if(instanceof<NodeFunc>(nodes[i])) {
            NodeFunc* nfunc = (NodeFunc*)nodes[i];

            if((hidePrivated && nfunc->isPrivate && !nfunc->isCtargs) || nfunc->isForwardDeclaration) continue;
            if(isImported && nfunc->isChecked) {
                nfunc->isChecked = false;
                if(nfunc->namespacesNames.size() > 0 ) {
                    nfunc->name = nfunc->origName;
                    nfunc->namespacesNames.clear();
                }
            }

            for(size_t i=0; i<names.size(); i++) nfunc->namespacesNames.insert(nfunc->namespacesNames.begin(), names[i]);
            nodes[i]->check();
        }
        else if(instanceof<NodeNamespace>(nodes[i])) {
            NodeNamespace* nnamespace = (NodeNamespace*)nodes[i];

            if(isImported && nnamespace->isChecked) {
                nnamespace->isChecked = false;
                // nnamespace->names.erase(nnamespace->names.begin());
            }
            else for(size_t i=0; i<names.size(); i++) nnamespace->names.insert(nnamespace->names.begin() + i, names[i]);

            nnamespace->isImported = (nnamespace->isImported || isImported);
            nodes[i]->check();
        }
        else if(instanceof<NodeComptime>(nodes[i])) {
            NodeComptime* ncomptime = (NodeComptime*)nodes[i];

            ncomptime->isImported = ncomptime->isImported || isImported;
            addNamespaceNames(ncomptime, names, isImported, isImported && ncomptime->isChecked);
        }
        else if(instanceof<NodeVar>(nodes[i])) {
            NodeVar* nvar = (NodeVar*)nodes[i];

            if(hidePrivated && nvar->isPrivate) continue;
            if(isImported && nvar->isChecked) {
                nvar->isChecked = false;
                nvar->name = nvar->origName;
                nvar->namespacesNames.clear();
            }

            for(size_t i=0; i<names.size(); i++) nvar->namespacesNames.insert(nvar->namespacesNames.begin(), names[i]);
            nodes[i]->check();
        }
        else if(instanceof<NodeStruct>(nodes[i])) {
            NodeStruct* structure = ((NodeStruct*)nodes[i]);

            if(isImported && structure->isChecked) {
                structure->isChecked = false;
                structure->name = structure->origname;
                structure->namespacesNames.clear();
            }

            for(size_t i=0; i<names.size(); i++) structure->namespacesNames.insert(structure->namespacesNames.begin(), names[i]);
            nodes[i]->check();
        }
        else if(instanceof<NodeAliasType>(nodes[i])) {
            NodeAliasType* naliastype = (NodeAliasType*)nodes[i];

            if(isImported && naliastype->isChecked) {
                naliastype->isChecked = false;
                naliastype->name = naliastype->origName;
                naliastype->namespacesNames.clear();
            }

            for(size_t i=0; i<names.size(); i++) naliastype->namespacesNames.insert(naliastype->namespacesNames.begin(), names[i]);
            nodes[i]->check();
        }
    }
}

RaveValue NodeNamespace::generate() {
    for(size_t i=0; i<nodes.size(); i++) {
        if(nodes[i] == nullptr) continue;

        if(instanceof<NodeFunc>(nodes[i])) {
            NodeFunc* nfunc = (NodeFunc*)nodes[i];

            if((hidePrivated && nfunc->isPrivate && !nfunc->isCtargs) || nfunc->isForwardDeclaration) continue;
            if(!nfunc->isChecked) {
                for(size_t i=0; i<names.size(); i++) nfunc->namespacesNames.insert(nfunc->namespacesNames.begin(), names[i]);
                nfunc->check();
            }

            nfunc->isExtern = (nfunc->isExtern || isImported);
            if(!isImported) nfunc->generate();
        }
        else if(instanceof<NodeNamespace>(nodes[i])) {
            NodeNamespace* nnamespace = (NodeNamespace*)nodes[i];

            if(!nnamespace->isChecked) {
                for(size_t i=0; i<names.size(); i++) nnamespace->names.insert(nnamespace->names.begin(), names[i]);
                nnamespace->check();
            }

            nnamespace->isImported = (nnamespace->isImported || isImported);
            nnamespace->generate();
        }
        else if(instanceof<NodeComptime>(nodes[i])) {
            NodeComptime* ncomptime = (NodeComptime*)nodes[i];

            addNamespaceNames(ncomptime, names, isImported, false);

            ncomptime->isImported = ncomptime->isImported || isImported;
            ncomptime->generate();
        }
        else if(instanceof<NodeVar>(nodes[i])) {
            NodeVar* nvar = (NodeVar*)nodes[i];

            if(hidePrivated && nvar->isPrivate) continue;
            if(!nvar->isChanged) {
                for(size_t i=0; i<names.size(); i++) nvar->namespacesNames.insert(nvar->namespacesNames.begin(), names[i]);
                nvar->check();
            }

            nvar->isExtern = (nvar->isExtern || isImported);
            nvar->generate();
        }
        else if(instanceof<NodeStruct>(nodes[i])) {
            NodeStruct* nstruct = (NodeStruct*)nodes[i];

            if(!nstruct->isChecked) {
                for(size_t i=0; i<names.size(); i++) nstruct->namespacesNames.insert(nstruct->namespacesNames.begin(), names[i]);
                nstruct->check();
            }

            nstruct->isImported = (nstruct->isImported || isImported);
            if(!nstruct->isImported) nstruct->generate();
        }
        else if(instanceof<NodeAliasType>(nodes[i])) {
            if(!nodes[i]->isChecked) {
                for(size_t i=0; i<names.size(); i++) ((NodeAliasType*)nodes[i])->namespacesNames.push_back(names[i]);
                nodes[i]->check();
            }

            nodes[i]->generate();
        }
    }

    return {};
}