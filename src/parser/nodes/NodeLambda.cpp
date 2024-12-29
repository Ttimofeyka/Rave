/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/utils.hpp"
#include "../../include/parser/nodes/NodeLambda.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"

NodeLambda::NodeLambda(int loc, TypeFunc* tf, NodeBlock* block, std::string name) {
    this->loc = loc;
    this->tf = tf;
    this->block = block;
    this->name = name;
}

NodeLambda::~NodeLambda() {
    if(tf != nullptr) delete tf;
    if(block != nullptr) delete block;
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

RaveValue NodeLambda::generate() {
    this->type = this->tf->main;

    const auto& activeLoops = generator->activeLoops;
    const auto builder = generator->builder;
    const auto currBB = generator->currBB;
    auto* const _scope = currScope;
    const auto& toReplace = generator->toReplace;

    const auto& _args = this->tf->args;
    std::vector<FuncArgSet> fas;
    fas.reserve(_args.size());

    for(const auto& arg : _args) fas.emplace_back(FuncArgSet{.name = arg->name, .type = arg->type});

    const auto lambdaId = generator->lambdas++;
    AST::lambdaTable["lambda" + std::to_string(lambdaId)] = this;

    auto nf = new NodeFunc("__RAVE_LAMBDA" + std::to_string(lambdaId), fas, block, false, {}, this->loc, this->type, {});
    nf->isComdat = true;
    nf->check();
    RaveValue func = nf->generate();

    generator->activeLoops = activeLoops;
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;
    generator->toReplace = toReplace;

    return func;
}