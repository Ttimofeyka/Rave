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

NodeConstStruct::NodeConstStruct(std::string name, std::vector<Node*> values, int loc) {
    this->structName = name;
    this->values = values;
    this->loc = loc;
}

NodeConstStruct::~NodeConstStruct() {
    for(int i=0; i<values.size(); i++) if(values[i] != nullptr) delete values[i];
}

Type* NodeConstStruct::getType() {return new TypeStruct(structName);}
Node* NodeConstStruct::comptime() {return this;}
Node* NodeConstStruct::copy() {return new NodeConstStruct(this->structName, this->values, this->loc);}
void NodeConstStruct::check() {this->isChecked = true;}

RaveValue NodeConstStruct::generate() {
    if(structName.find('<') != std::string::npos && AST::structTable.find(structName) == AST::structTable.end()) {
        std::string sTypes = structName.substr(structName.find('<') + 1, structName.find('>') - structName.find('<') - 1);

        Lexer tLexer(sTypes, 1);
        Parser tParser(tLexer.tokens, "(builtin)");
        std::vector<Type*> types;

        while(tParser.peek()->type != TokType::Eof) {
            switch(tParser.peek()->type) {
                case TokType::Number: case TokType::HexNumber: case TokType::FloatNumber: {
                    Node* value = tParser.parseExpr();
                    types.push_back(new TypeTemplateMember(value->getType(), value));
                    break;
                }
                default:
                    types.push_back(tParser.parseType(true));
                    break;
            }

            if(tParser.peek()->type == TokType::Comma) tParser.next();
        }

        AST::structTable[structName.substr(0, structName.find('<'))]->genWithTemplate("<" + sTypes + ">", types);
    }

    LLVMTypeRef constStructType = generator->genType(new TypeStruct(structName), this->loc);
    std::vector<RaveValue> llvmValues;

    bool isConst = true;

    for(int i=0; i<this->values.size(); i++) {
        RaveValue generated = this->values[i]->generate();

        if(!LLVMIsConstant(generated.value)) isConst = false;

        llvmValues.push_back(generated);
    }

    if(AST::structTable.find(structName) == AST::structTable.end()) generator->error("structure \033[1m" + structName + "\033[22m does not exist!", loc);

    NodeStruct* structNode = AST::structTable[structName];
    std::vector<NodeVar*>& variables = structNode->variables;

    if(isConst) {
        std::vector<LLVMValueRef> __data;
        for(int i=0; i<llvmValues.size(); i++) {
            if(variables[i]->getType()->toString() != llvmValues[i].type->toString()) {
                Type* varType = variables[i]->getType();

                if(instanceof<TypeBasic>(varType) && instanceof<TypeBasic>(llvmValues[i].type)) LLVM::cast(llvmValues[i], varType, loc);
                else generator->error("incompatible types in constant structure: value of type '" + llvmValues[i].type->toString() + "' trying to be assigned to variable named '" + variables[i]->name + "' of type '" + varType->toString() + "'!", loc);
            }

            __data.push_back(llvmValues[i].value);
        }

        return {LLVMConstNamedStruct(constStructType, __data.data(), __data.size()), this->getType()};
    }
    else {
        if(currScope == nullptr) generator->error("constant structure with dynamic values cannot be created outside a function!", loc);
        RaveValue temp = LLVM::alloc(new TypeStruct(this->structName), "constStruct_temp");

        for(int i=0; i<this->values.size(); i++) {
            if(variables[i]->getType()->toString() != llvmValues[i].type->toString()) {
                Type* varType = variables[i]->getType();

                if(instanceof<TypeBasic>(varType) && instanceof<TypeBasic>(llvmValues[i].type)) LLVM::cast(llvmValues[i], varType, loc);
                else generator->error("incompatible types in constant structure: value of type \033[1m" + llvmValues[i].type->toString() + "\033[22m trying to be assigned to variable named \033[1m" + variables[i]->name + "\033[22m of type \033[1m" + varType->toString() + "\033[22m!", loc);
            }

            Binary::operation(TokType::Equ, new NodeGet(new NodeDone(temp), variables[i]->name, true, this->loc), new NodeDone(llvmValues[i]), loc);
        }

        return LLVM::load(temp, "constStruct_tempLoad", loc);
    }
}