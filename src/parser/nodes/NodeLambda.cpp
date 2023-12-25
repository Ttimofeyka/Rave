/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/utils.hpp"
#include "../../include/parser/nodes/NodeLambda.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"

NodeLambda::NodeLambda(long loc, TypeFunc* tf, NodeBlock* block, std::string name) {
    this->loc = loc;
    this->tf = tf;
    this->block = block;
    this->name = name;
}

Type* NodeLambda::getType() {return (Type*)this->tf;}
Node* NodeLambda::copy() {return new NodeLambda(loc, (TypeFunc*)this->tf->copy(), (NodeBlock*)this->block->copy(), this->name);}
Node* NodeLambda::comptime() {return this;}
void NodeLambda::check() {}

std::vector<LLVMTypeRef> NodeLambda::generateTypes() {
    std::vector<LLVMTypeRef> buffer;
    for(int i=0; i<this->tf->args.size(); i++) buffer.push_back(generator->genType(this->tf->args[i], this->loc));
    return buffer;
}

LLVMValueRef NodeLambda::generate() {
    this->type = this->tf->main;

    std::map<int32_t, Loop> activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    LLVMBuilderRef builder = generator->builder;
    LLVMBasicBlockRef currBB = generator->currBB;
    Scope* _scope = currScope;
    std::map<std::string, Type*> toReplace = std::map<std::string, Type*>(generator->toReplace);

    std::vector<TypeFuncArg*> _args = std::vector<TypeFuncArg*>(this->tf->args);
    std::vector<FuncArgSet> fas;

    for(int i=0; i<_args.size(); i++) fas.push_back(FuncArgSet{.name = _args[i]->name, .type = _args[i]->type});

    AST::lambdaTable["lambda"+std::to_string(generator->lambdas)] = this;
    NodeFunc* nf = new NodeFunc("__RAVE_LAMBDA"+std::to_string(generator->lambdas), fas, block, false, {}, this->loc, this->tf->main, {});
    nf->check();
    nf->isComdat = true;
    LLVMValueRef func = nf->generate();

    generator->lambdas += 1;

    generator->activeLoops = std::map<int32_t, Loop>(activeLoops);
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;
    generator->toReplace = std::map<std::string, Type*>(toReplace);

    return func;
}