#include <llvm-c/Target.h>
#include "./include/parser/ast.hpp"
#include "./include/parser/Types.hpp"
#include "./include/parser/nodes/NodeIndex.hpp"
#include "./include/parser/nodes/NodeBuiltin.hpp"
#include "./include/parser/nodes/NodeCast.hpp"
#include "./include/parser/nodes/NodeBool.hpp"
#include "./include/parser/nodes/NodeDone.hpp"
#include "./include/parser/nodes/NodeStruct.hpp"
#include "./include/parser/nodes/NodeCall.hpp"
#include "./include/parser/nodes/NodeFunc.hpp"
#include "./include/parser/nodes/NodeUnary.hpp"
#include "./include/parser/nodes/NodeType.hpp"
#include "./include/parser/nodes/NodeIf.hpp"
#include "./include/parser/nodes/NodeWhile.hpp"
#include "./include/parser/nodes/NodeRet.hpp"
#include "./include/parser/nodes/NodeBinary.hpp"
#include "./include/parser/nodes/NodeIden.hpp"
#include "./include/parser/nodes/NodeInt.hpp"
#include "./include/parser/nodes/NodeFloat.hpp"
#include "./include/parser/nodes/NodeString.hpp"
#include "./include/parser/nodes/NodeNull.hpp"
#include "./include/parser/nodes/NodeGet.hpp"
#include "./include/parser/nodes/NodeVar.hpp"
#include "./include/lexer/tokens.hpp"
#include "./include/compiler.hpp"
#include <iostream>

namespace {
    RaveValue handleBinaryAssignment(Node* first, Node* second, char op, int loc) {
        if ((instanceof<NodeCast>(second)) && instanceof<TypeVoid>(second->getType())) generator->error("an attempt to store a \033[1mvoid\033[22m value to the variable!", loc);

        if (instanceof<NodeIden>(first)) {
            NodeIden* id = ((NodeIden*)first);

            if (AST::aliasTable.find(id->name) != AST::aliasTable.end()) {
                AST::aliasTable[id->name] = second->copy();
                return {};
            }

            if (currScope->locatedAtThis(id->name)) return Binary::operation(TokType::Equ, new NodeGet(new NodeIden("this", loc), id->name, true, id->loc), second, loc);

            NodeVar* nvar = currScope->getVar(id->name, loc);

            if (nvar->isConst && nvar->isChanged && id->name != "this") generator->error("an attempt to change the value of a constant variable!", loc);
            else if (!nvar->isChanged) currScope->hasChanged(id->name);

            if (currScope->localVars.find(id->name) != currScope->localVars.end()) currScope->localVars[id->name]->isUsed = true;

            RaveValue vFirst = currScope->getWithoutLoad(id->name, loc);
            RaveValue vSecond = second->generate();

            if (instanceof<NodeNull>(second)) ((NodeNull*)second)->type = vFirst.type->getElType();

            if (instanceof<TypeStruct>(nvar->type) || (instanceof<TypePointer>(nvar->type) && instanceof<TypeStruct>(nvar->type->getElType()))
               && !instanceof<TypeStruct>(second->getType()) || (instanceof<TypePointer>(second->getType()) && instanceof<TypeStruct>(second->getType()->getElType()))
            ) {
                std::pair<std::string, std::string> opOverload = Binary::isOperatorOverload(first, second, vFirst, vSecond, op);

                if (opOverload.first != "") {
                    NodeCall* _overloadedCall = new NodeCall(loc,
                        new NodeIden(AST::structTable[opOverload.first[0] == '!' ? opOverload.first.substr(1) : opOverload.first]->operators[op][opOverload.second]->name, loc),
                        std::vector<Node*>({new NodeDone(vFirst), new NodeDone(vSecond)})
                    );

                    return opOverload.first[0] == '!' ? Unary::make(loc, TokType::Ne, _overloadedCall) : _overloadedCall->generate();
                }
                else if (instanceof<TypeBasic>(second->getType()))
                    generator->error("an attempt to change value of the structure as the variable \033[1m" + id->name + "\033[22m without overloading!", loc);
            }

            if (vSecond.type && vSecond.type->toString() == vFirst.type->toString()) vSecond = LLVM::load(vSecond, "NodeBinary_NodeIden_load", loc);

            if (instanceof<TypeArray>(vFirst.type->getElType()) && instanceof<TypePointer>(vSecond.type)) generator->error("cannot store a value of type \033[1m" + vSecond.type->toString() + "\033[22m into a variable of type \033[1m" + vFirst.type->getElType()->toString() + "\033[22m!", loc);

            Binary::store(vFirst, vSecond, loc);
            return {};
        }
        else if (instanceof<NodeGet>(first)) {
            NodeGet* nget = (NodeGet*)first;
            if (nget->elementIsConst) generator->error("An attempt to change the constant element!", loc);

            nget->isMustBePtr = true;

            RaveValue ptr = nget->generate();
            RaveValue value = second->generate();

            if (instanceof<TypeStruct>(first->getType()) || (instanceof<TypePointer>(first->getType()) && instanceof<TypeStruct>(first->getType()->getElType()))
               && !instanceof<TypeStruct>(second->getType()) || (instanceof<TypePointer>(second->getType()) && instanceof<TypeStruct>(second->getType()->getElType()))
            ) {
                std::pair<std::string, std::string> opOverload = Binary::isOperatorOverload(first, second, ptr, value, op);

                if (opOverload.first != "") {
                    NodeCall* _overloadedCall = new NodeCall(loc,
                        new NodeIden(AST::structTable[opOverload.first[0] == '!' ? opOverload.first.substr(1) : opOverload.first]->operators[op][opOverload.second]->name, loc),
                        std::vector<Node*>({new NodeDone(ptr), new NodeDone(value)})
                    );

                    return opOverload.first[0] == '!' ? Unary::make(loc, TokType::Ne, _overloadedCall) : _overloadedCall->generate();
                }
            }

            Binary::store(ptr, value, loc);
            return {};
        }
        else if (instanceof<NodeIndex>(first)) {
            NodeIndex* ind = (NodeIndex*)first;
            ind->isMustBePtr = true;

            if (instanceof<NodeIden>(ind->element) && currScope->locatedAtThis(((NodeIden*)ind->element)->name)) {
                ind->element = new NodeGet(new NodeIden("this", loc), ((NodeIden*)ind->element)->name, true, ((NodeIden*)ind->element)->loc);
                return Binary::operation(op, first, second, loc);
            }

            RaveValue value = {nullptr, nullptr};

            RaveValue ptr = ind->generate();
            if (ind->elementIsConst) generator->error("An attempt to change the constant element!", loc);

            // Check for TypeVector
            if (instanceof<TypeVector>(ind->element->getType())) {
                Type* elType = ((TypeVector*)ind->element->getType())->getElType();
                RaveValue number = second->generate();

                if (instanceof<TypeBasic>(number.type) && ((TypeBasic*)number.type)->type != ((TypeBasic*)elType)->type) LLVM::cast(number, elType, loc);

                if (number.type->toString() != elType->toString()) generator->error("cannot store a value of type \033[1m" + number.type->toString() + "\033[22m into a value of type \033[1m" + elType->toString() + "\033[22m!", loc);

                if (instanceof<TypePointer>(ptr.type)) {
                    value = LLVM::load(ptr, "NodeBinary_TypeVector_load", loc);
                    value.value = LLVMBuildInsertElement(generator->builder, value.value, number.value, ind->indexes[0]->generate().value, "NodeBinary_TypeVector_insert");
                }
                else value = {LLVMBuildInsertElement(generator->builder, value.value, number.value, ind->indexes[0]->generate().value, "NodeBinary_TypeVector_insert"), ptr.type};
            }
            else value = second->generate();

            if (instanceof<NodeNull>(second)) ((NodeNull*)second)->type = ptr.type->getElType();

            Binary::store(ptr, value, loc);
            return {};
        }
        else if (instanceof<NodeDone>(first)) {
            Binary::store(first->generate(), second->generate(), loc);
            return {};
        }

        return {};
    }

    RaveValue handleIn(Node* first, Node* second, RaveValue vFirst, RaveValue vSecond, char op, int loc) {
        char connect = (op == TokType::In ? TokType::Or : TokType::And);
        char exprOp = (op == TokType::In ? TokType::Equal : TokType::Nequal);

        if (instanceof<TypeStruct>(vSecond.type->getElType())) {
            auto overload = Binary::isOperatorOverload(first, second, vSecond, vFirst, TokType::In);

            if (overload.first != "" && overload.second != "") {
                NodeFunc* _operator = AST::structTable[overload.first]->operators[TokType::In][overload.second];

                if (_operator == nullptr) generator->error("operator not found!", loc);
                else {
                    NodeCall* _call = new NodeCall(loc, new NodeIden(_operator->name, loc), std::vector<Node*>({new NodeDone(vSecond), new NodeDone(vFirst)}));
                    return (op == TokType::NeIn ? Unary::make(loc, TokType::Ne, _call) : _call->generate());
                }
            }
        }

        if (instanceof<TypePointer>(vSecond.type) && instanceof<TypeArray>(vSecond.type->getElType())) vSecond = LLVM::load(vSecond, "NodeBinary_loadS", loc);
        else if (!instanceof<TypeArray>(vSecond.type)) generator->error("array expected!", loc);

        uint32_t length = LLVMGetArrayLength(LLVMTypeOf(vSecond.value));
        Type* arrayType = second->getType()->getElType();

        if (length == 0) generator->error("array is empty!", loc);

        NodeBinary* out = new NodeBinary(exprOp, first, new NodeDone({LLVMBuildExtractValue(generator->builder, vSecond.value, 0, "NodeBinary_extract"), arrayType}), loc);

        for (size_t i=1; i<length; i++)
            out = new NodeBinary(connect, out, new NodeBinary(exprOp, first, new NodeDone({LLVMBuildExtractValue(generator->builder, vSecond.value, i, "NodeBinary_extract"), arrayType}), loc), loc);

        return (out->generate());
    }

    RaveValue handleLogicalOperators(Node* first, Node* second, char op, int loc) {
        RaveValue vFirst = first->generate();
        LLVMBasicBlockRef ascendBlock = LLVM::makeBlock("ascend", currScope->funcName); // Further calculations are necessary if reached
        LLVMBasicBlockRef mergeBlock = LLVM::makeBlock("merge", currScope->funcName);

        if (op == TokType::And) LLVMBuildCondBr(generator->builder, vFirst.value, ascendBlock, mergeBlock);
        else LLVMBuildCondBr(generator->builder, vFirst.value, mergeBlock, ascendBlock);
        LLVMBasicBlockRef shortcutBlock = LLVMGetInsertBlock(generator->builder);

        LLVM::Builder::atEnd(ascendBlock);
        RaveValue vSecond = second->generate();
        LLVMBuildBr(generator->builder, mergeBlock);
        ascendBlock = LLVMGetInsertBlock(generator->builder);

        LLVM::Builder::atEnd(mergeBlock);
        LLVMValueRef phi = LLVMBuildPhi(generator->builder, LLVMInt1TypeInContext(generator->context), "phi");
        LLVMValueRef incomingValues[] = {vSecond.value, vFirst.value};
        LLVMBasicBlockRef incomingBlocks[] = {ascendBlock, shortcutBlock};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        
        return {phi, basicTypes[BasicType::Bool]};
    }

    RaveValue handleRem(Node* first, Node* second, int loc) {
        RaveValue _first = first->generate();
        RaveValue _second = second->generate();

        if (instanceof<TypePointer>(_first.type)) _first = LLVM::load(_first, "Binary_Rem_load", loc);
        if (instanceof<TypePointer>(_second.type)) _second = LLVM::load(_second, "Binary_Rem_load2", loc);

        LLVM::cast(_second, _first.type, loc);

        if (isFloatType(_first.type)) return (new NodeBuiltin("fmodf", {new NodeDone(_first), new NodeDone(_second)}, loc, nullptr))->generate();

        if (((TypeBasic*)_first.type)->isUnsigned()) return {LLVMBuildURem(generator->builder, _first.value, _second.value, "LLVM_urem"), _first.type};
        return {LLVMBuildSRem(generator->builder, _first.value, _second.value, "LLVM_srem"), _first.type};
    }
}

void Binary::store(RaveValue pointer, RaveValue value, int loc) {
    if (pointer.type == nullptr || pointer.value == nullptr || value.type == nullptr || value.value == nullptr) return;

    Type* memType = pointer.type->getElType();

    if (instanceof<TypeBasic>(memType)) {
        if (instanceof<TypeBasic>(value.type)) {
            TypeBasic* memBasic = (TypeBasic*)memType;
            TypeBasic* valBasic = (TypeBasic*)value.type;

            if (memBasic->type != valBasic->type) LLVM::cast(value, memType, loc);
        }
        else if (!instanceof<TypeStruct>(value.type)) {
            generator->error("cannot store a value of type \033[1m" + value.type->toString() + "\033[22m into a value of type \033[1m" + memType->toString() + "\033[22m!", loc);
            return;
        }
    }
    else if (instanceof<TypePointer>(memType)) {
        if (instanceof<TypeBasic>(value.type)) {
            generator->error("cannot store a value of type \033[1m" + value.type->toString() + "\033[22m into a value of type \033[1m" + memType->toString() + "\033[22m!", loc);
            return;
        }
    }

    LLVMBuildStore(generator->builder, value.value, pointer.value);
}

std::pair<std::string, std::string> Binary::isOperatorOverload(Node* firstNode, Node* secondNode, RaveValue first, RaveValue second, char op) {
    if (first.value == nullptr || second.value == nullptr) return {"", ""};

    Type* type = first.type;

    if (instanceof<TypeStruct>(type) || (instanceof<TypePointer>(type) && instanceof<TypeStruct>(type->getElType()))) {
        std::string structName = (instanceof<TypeStruct>(type) ? ((TypeStruct*)type)->name : ((TypeStruct*)(((TypePointer*)type)->instance))->name);
        if (AST::structTable.find(structName) != AST::structTable.end()) {
            auto& operators = AST::structTable[structName]->operators;
            if (operators.find(op) != operators.end()) {
                std::vector<Type*> types = {firstNode->getType(), secondNode->getType()};
                std::string sTypes = typesToString(types);

                if (operators[op].find(sTypes) != operators[op].end()) return {structName, sTypes};
                else if (op == TokType::In) return {structName, operators[op].begin()->second->name.substr(operators[op].begin()->second->name.find('['))};
            }
            else if (op == TokType::Nequal && operators.find(TokType::Equal) != operators.end()) {
                std::vector<Type*> types = {firstNode->getType(), secondNode->getType()};
                std::string sTypes = typesToString(types);
                if (operators[TokType::Equal].find(sTypes) != operators[TokType::Equal].end()) return {"!" + structName, sTypes};
            }
        }
    }

    return {"", ""};
}

RaveValue Binary::operation(char op, Node* first, Node* second, int loc) {
    if (first == nullptr || second == nullptr) return {};

    if (op == TokType::PluEqu || op == TokType::MinEqu || op == TokType::MulEqu || op == TokType::DivEqu)
        return Binary::operation(TokType::Equ, first, new NodeBinary(op == TokType::PluEqu ? TokType::Plus : (op == TokType::MinEqu ? TokType::Minus : (op == TokType::MulEqu ? TokType::Multiply : TokType::Divide)), first->copy(), second, loc), loc);

    if (op == TokType::Equ) return handleBinaryAssignment(first, second, op, loc);

    if (instanceof<NodeNull>(first)) ((NodeNull*)first)->type = second->getType();
    else if (instanceof<NodeNull>(second)) ((NodeNull*)second)->type = first->getType();   

    if (op == TokType::Rem) return handleRem(first, second, loc);
    if (op == TokType::And || op == TokType::Or) return handleLogicalOperators(first, second, op, loc);

    RaveValue vFirst = first->generate();
    RaveValue vSecond = second->generate();

    while (instanceof<TypeConst>(vFirst.type)) vFirst.type = vFirst.type->getElType();
    while (instanceof<TypeConst>(vSecond.type)) vSecond.type = vSecond.type->getElType();

    if (instanceof<TypeStruct>(first->getType()) || instanceof<TypePointer>(first->getType())
     && !instanceof<TypeStruct>(second->getType()) && !instanceof<TypePointer>(second->getType())) {
        std::pair<std::string, std::string> opOverload = Binary::isOperatorOverload(first, second, vFirst, vSecond, op);

        if (opOverload.first != "") {
            bool isNegative = opOverload.first[0] == '!';

            NodeCall* _overloadedCall = new NodeCall(loc,
                new NodeIden(AST::structTable[isNegative ? opOverload.first.substr(1) : opOverload.first]->operators[isNegative ? TokType::Equal : op][opOverload.second]->name, loc),
                std::vector<Node*>({new NodeDone(vFirst), new NodeDone(vSecond)})
            );

            return isNegative ? Unary::make(loc, TokType::Ne, _overloadedCall) : _overloadedCall->generate();
        }
    }

    if (instanceof<TypePointer>(vFirst.type) && !instanceof<TypePointer>(vSecond.type)) vFirst = LLVM::load(vFirst, "NodeBinary_loadF", loc);
    else if (instanceof<TypePointer>(vSecond.type) && !instanceof<TypePointer>(vFirst.type)) vSecond = LLVM::load(vSecond, "NodeBinary_loadS", loc);

    Types::replaceTemplates(&vFirst.type);
    Types::replaceTemplates(&vSecond.type);

    if (instanceof<TypeBasic>(vFirst.type) && instanceof<TypeBasic>(vSecond.type)) LLVM::castForExpression(vFirst, vSecond);

    if (op == TokType::In || op == TokType::NeIn) return handleIn(first, second, vFirst, vSecond, op, loc);

    if (vFirst.type->toString() != vSecond.type->toString()) {
        if (!instanceof<TypeBasic>(vFirst.type) || !instanceof<TypeBasic>(vSecond.type)) generator->error("value types \033[1m" + vFirst.type->toString() + "\033[22m and \033[1m" + vSecond.type->toString() + "\033[22m are incompatible!", loc);
    }

    if (instanceof<TypeVector>(vFirst.type) && instanceof<TypeVector>(vSecond.type)) {
        if (op == TokType::And || op == TokType::Or || op == TokType::BitXor) {
            if (isFloatType((vFirst.type->getElType()))) {
                // Cast both vectors to int and then back
                Type* oldType = vFirst.type;

                LLVM::cast(vFirst, new TypeVector(basicTypes[BasicType::Int], ((TypeVector*)vFirst.type)->count), loc);
                LLVM::cast(vSecond, vFirst.type);

                RaveValue result = {(op == TokType::And ? LLVMBuildAnd(generator->builder, vFirst.value, vSecond.value, "NodeBinary_and") :
                    (op == TokType::Or ? LLVMBuildOr(generator->builder, vFirst.value, vSecond.value, "NodeBinary_or") :
                    LLVMBuildXor(generator->builder, vFirst.value, vSecond.value, "NodeBinary_xor"))
                ), vFirst.type};

                LLVM::cast(result, oldType, loc);
            }
        }
    }

    switch (op) {
        case TokType::Plus: return LLVM::sum(vFirst, vSecond);
        case TokType::Minus: return LLVM::sub(vFirst, vSecond);
        case TokType::Multiply: return LLVM::mul(vFirst, vSecond);
        case TokType::Divide: return LLVM::div(vFirst, vSecond, (instanceof<TypeBasic>(first->getType()) && ((TypeBasic*)first->getType())->isUnsigned()));
        case TokType::Equal: case TokType::Nequal:
        case TokType::Less: case TokType::More:
        case TokType::LessEqual: case TokType::MoreEqual: return LLVM::compare(vFirst, vSecond, op);
        case TokType::Amp: return {LLVMBuildAnd(generator->builder, vFirst.value, vSecond.value, "NodeBinary_and"), vFirst.type};
        case TokType::BitOr: return {LLVMBuildOr(generator->builder, vFirst.value, vSecond.value, "NodeBinary_or"), vFirst.type};
        case TokType::BitXor: return {LLVMBuildXor(generator->builder, vFirst.value, vSecond.value, "NodeBinary_xor"), vFirst.type};
        case TokType::BitLeft: return {LLVMBuildShl(generator->builder, vFirst.value, vSecond.value, "NodeBinary_and"), vFirst.type};
        case TokType::BitRight: return {LLVMBuildLShr(generator->builder, vFirst.value, vSecond.value, "NodeBinary_and"), vFirst.type};
        default: generator->error("undefined operator \033[1m" + std::to_string(op) + "\033[22m!", loc); return {};
    }
}
