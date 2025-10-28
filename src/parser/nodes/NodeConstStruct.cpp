/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeConstStruct.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"
#include "../../include/lexer/lexer.hpp"

// TODO: Recheck

NodeConstStruct::NodeConstStruct(std::string name, std::vector<Node*> values, int loc) 
    : structName(name), values(values), loc(loc) {}

NodeConstStruct::~NodeConstStruct() {
    for (Node* value : values) if (value) delete value;
}

Type* NodeConstStruct::getType() {return new TypeStruct(structName);}
Node* NodeConstStruct::comptime() {return this;}
Node* NodeConstStruct::copy() {return new NodeConstStruct(this->structName, this->values, this->loc);}
void NodeConstStruct::check() {isChecked = true;}

RaveValue NodeConstStruct::generate() {
    if (structName.find('<') != std::string::npos && AST::structTable.find(structName) == AST::structTable.end()) {
        const size_t lt = structName.find('<');
        std::string sTypes = structName.substr(lt + 1, structName.find('>') - lt - 1);
        Lexer tLexer(sTypes, 1);
        Parser tParser(tLexer.tokens, "(builtin)");
        std::vector<Type*> types;

        while (tParser.peek()->type != TokType::Eof) {
            if (tParser.peek()->type >= TokType::Number && tParser.peek()->type <= TokType::HexNumber) {
                Node* value = tParser.parseExpr();
                types.push_back(new TypeTemplateMember(value->getType(), value));
            }
            else types.push_back(tParser.parseType(true));

            if (tParser.peek()->type == TokType::Comma) tParser.next();
        }

        AST::structTable[structName.substr(0, lt)]->genWithTemplate("<" + sTypes + ">", types);
    }

    if (AST::structTable.find(structName) == AST::structTable.end())
        generator->error("structure \033[1m" + structName + "\033[22m does not exist!", loc);

    LLVMTypeRef constStructType = generator->genType(new TypeStruct(structName), loc);
    std::vector<RaveValue> llvmValues;
    std::vector<LLVMValueRef> __data;
    bool isConst = true;
    
    for (Node* value : values) {
        RaveValue generated = value->generate();
        isConst &= LLVMIsConstant(generated.value);
        llvmValues.push_back(generated);
    }

    auto validateAndCast = [&](size_t i) {
        Type* varType = AST::structTable[structName]->variables[i]->getType();
        if (varType->toString() != llvmValues[i].type->toString()) {
            if (instanceof<TypeBasic>(varType) && instanceof<TypeBasic>(llvmValues[i].type))
                LLVM::cast(llvmValues[i], varType, loc);
            else generator->error("incompatible types in constant structure: value of type \033[1m" + llvmValues[i].type->toString() + "\033[22m trying to be assigned to variable named \033[1m" + AST::structTable[structName]->variables[i]->name + "\033[22m of type \033[1m" + varType->toString() + "\033[22m!", loc);
        }
    };

    if (isConst) {
        for (size_t i=0; i<llvmValues.size(); i++) {
            validateAndCast(i);
            __data.push_back(llvmValues[i].value);
        }
        return { LLVMConstNamedStruct(constStructType, __data.data(), __data.size()), getType() };
    }

    if (!currScope) generator->error("constant structure with dynamic values cannot be created outside a function!", loc);
    RaveValue temp = LLVM::alloc(new TypeStruct(structName), "constStruct_temp");

    for (size_t i=0; i<llvmValues.size(); i++) {
        validateAndCast(i);
        Binary::operation(TokType::Equ, new NodeGet(new NodeDone(temp), AST::structTable[structName]->variables[i]->name, true, loc), new NodeDone(llvmValues[i]), loc);
    }

    return LLVM::load(temp, "constStruct_tempLoad", loc);
}
