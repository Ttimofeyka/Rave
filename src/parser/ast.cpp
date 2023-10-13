/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/ast.hpp"
#include "../include/parser/Types.hpp"
#include "../include/llvm-c/Target.h"
#include "../include/llvm-c/Analysis.h"
#include "../include/parser/nodes/NodeFunc.hpp"
#include "../include/parser/nodes/NodeVar.hpp"
#include "../include/parser/nodes/NodeRet.hpp"
#include "../include/parser/nodes/NodeLambda.hpp"
#include "../include/parser/nodes/NodeStruct.hpp"
#include <iostream>

std::map<std::string, Type*> AST::aliasTypes;
std::map<std::string, Node*> AST::aliasTable;
std::map<std::string, NodeVar*> AST::varTable;
std::map<std::string, NodeFunc*> AST::funcTable;
std::map<std::string, NodeLambda*> AST::lambdaTable;
std::map<std::string, NodeStruct*> AST::structTable;
std::map<std::pair<std::string, std::string>, NodeFunc*> AST::methodTable;
std::map<std::pair<std::string, std::string>, StructMember> AST::structsNumbers;
std::vector<std::string> AST::importedFiles;
std::map<std::string, std::vector<Node*>> AST::parsed;
std::map<int, Node*> AST::condStack;
std::string AST::mainFile;

LLVMGen* generator = nullptr;
Scope* currScope;
LLVMTargetDataRef dataLayout;

std::vector<Type*> parametersToTypes(std::vector<LLVMValueRef> params) {
    if(params.empty()) return {};
    std::vector<Type*> types;

    for(int i=0; i<params.size(); i++) {
        if(params[i] == nullptr) continue;
        LLVMTypeRef t = LLVMTypeOf(params[i]);
        if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
            if(LLVMGetTypeKind(LLVMGetElementType(t)) == LLVMStructTypeKind) types.push_back(new TypeStruct(std::string(LLVMGetStructName(LLVMGetElementType(t)))));
            else types.push_back(new TypePointer(nullptr));
        }
        else if(LLVMGetTypeKind(t) == LLVMIntegerTypeKind) {
            if(t == LLVMInt32TypeInContext(generator->context)) types.push_back(new TypeBasic(BasicType::Int));
            else if(t == LLVMInt64TypeInContext(generator->context)) types.push_back(new TypeBasic(BasicType::Long));
            else if(t == LLVMInt16TypeInContext(generator->context)) types.push_back(new TypeBasic(BasicType::Short));
            else if(t == LLVMInt8TypeInContext(generator->context)) types.push_back(new TypeBasic(BasicType::Char));
            else if(t == LLVMInt1TypeInContext(generator->context)) types.push_back(new TypeBasic(BasicType::Bool));
            else types.push_back(new TypeBasic(BasicType::Cent));
        }
        else if(LLVMGetTypeKind(t) == LLVMFloatTypeKind) types.push_back(new TypeBasic(BasicType::Float));
        else if(LLVMGetTypeKind(t) == LLVMDoubleTypeKind) types.push_back(new TypeBasic(BasicType::Double));
        else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) types.push_back(new TypeStruct(std::string(LLVMGetStructName(t))));
        else if(LLVMGetTypeKind(t) == LLVMArrayTypeKind) types.push_back(new TypeArray(0, nullptr));
    }
    return types;
}

std::string typesToString(std::vector<FuncArgSet> args) {
    std::string data = "[";
    for(int i=0; i<args.size(); i++) {
        Type* ty = args[i].type;
        if(instanceof<TypeBasic>(ty)) {
            switch(((TypeBasic*)ty)->type) {
                case BasicType::Float: data += "_f"; break;
                case BasicType::Double: data += "_d"; break;
                case BasicType::Int: data += "_i"; break;
                case BasicType::Cent: data += "_t"; break;
                case BasicType::Char: data += "_c"; break;
                case BasicType::Long: data += "_l"; break;
                case BasicType::Short: data += "_h"; break;
                case BasicType::Ushort: data += "_uh"; break;
                case BasicType::Uchar: data += "_uc"; break;
                case BasicType::Uint: data += "_ui"; break;
                case BasicType::Ulong: data += "_ul"; break;
                case BasicType::Ucent: data += "_ut"; break;
                default: data += "_b"; break;
            }
        }
        else if(instanceof<TypePointer>(ty)) {
            if(instanceof<TypeStruct>(((TypePointer*)ty)->instance)) {
                TypeStruct* ts = (TypeStruct*)(((TypePointer*)ty)->instance);
                if(ts->name.find('<') == std::string::npos) data += "_s-"+ts->name;
                else data += "_s-"+ts->name.substr(0,ts->name.find('<'));
            }
            else data += "_p";
        }
        else if(instanceof<TypeArray>(ty)) data += "_a";
        else if(instanceof<TypeStruct>(ty)) {
            if(((TypeStruct*)ty)->name.find('<') == std::string::npos) data += "_s-"+((TypeStruct*)ty)->name;
            else data += "_s-"+((TypeStruct*)ty)->name.substr(0,((TypeStruct*)ty)->name.find('<'));
        }
        else if(instanceof<TypeFunc>(ty)) data += "_func";
        else if(instanceof<TypeConst>(ty)) {
            std::string constVal = typesToString({FuncArgSet{.name = "", .type = ((TypeConst*)ty)->instance}});
            data += constVal.substr(1, constVal.size()-1);
        }
    }
    return data + "]";
}

std::string typesToString(std::vector<Type*> args) {
    std::string data = "[";
    for(int i=0; i<args.size(); i++) {
        if(instanceof<TypeBasic>(args[i])) {
            switch(((TypeBasic*)args[i])->type) {
                case BasicType::Float: data += "_f"; break;
                case BasicType::Double: data += "_d"; break;
                case BasicType::Int: data += "_i"; break;
                case BasicType::Cent: data += "_t"; break;
                case BasicType::Char: data += "_c"; break;
                case BasicType::Long: data += "_l"; break;
                case BasicType::Short: data += "_h"; break;
                case BasicType::Ushort: data += "_uh"; break;
                case BasicType::Uchar: data += "_uc"; break;
                case BasicType::Uint: data += "_ui"; break;
                case BasicType::Ulong: data += "_ul"; break;
                case BasicType::Ucent: data += "_ut"; break;
                default: data += "_b"; break;
            }
        }
        else if(instanceof<TypePointer>(args[i])) {
            if(instanceof<TypeStruct>(((TypePointer*)args[i])->instance)) {
                TypeStruct* ts = (TypeStruct*)(((TypePointer*)args[i])->instance);
                if(ts->name.find('<') == std::string::npos) data += "_s-"+ts->name;
                else data += "_s-"+ts->name.substr(0,ts->name.find('<'));
            }
            else data += "_p";
        }
        else if(instanceof<TypeArray>(args[i])) data += "_a";
        else if(instanceof<TypeStruct>(args[i])) {
            if(((TypeStruct*)args[i])->name.find('<') == std::string::npos) data += "_s-"+((TypeStruct*)args[i])->name;
            else data += "_s-"+((TypeStruct*)args[i])->name.substr(0,((TypeStruct*)args[i])->name.find('<'));
        }
        else if(instanceof<TypeFunc>(args[i])) data += "_func";
        else if(instanceof<TypeConst>(args[i])) {
            std::string constVal = typesToString({FuncArgSet{.name = "", .type = ((TypeConst*)args[i])->instance}});
            data += constVal.substr(1, constVal.size()-1);
        }
    }
    return data + "]";
}

void AST::checkError(std::string message, long loc) {
    std::cout << "\033[0;31mError on "+std::to_string(loc)+" line: "+message+"\033[0;0m\n";
	exit(1);
}

LLVMGen::LLVMGen(std::string file, genSettings settings) {
    this->file = file;
    this->settings = settings;
    this->context = LLVMContextCreate();
    this->lModule = LLVMModuleCreateWithNameInContext("rave", this->context);
    LLVMContextSetOpaquePointers(this->context, 0);

    this->neededFunctions["free"] = "std::free";
    this->neededFunctions["malloc"] = "std::malloc";
    this->neededFunctions["assert"] = "std::assert[_b_p]";
}

void LLVMGen::error(std::string msg, long line) {
    std::cout << "\033[0;31mError in '"+this->file+"' file at "+std::to_string(line)+" line: "+msg+"\033[0;0m" << std::endl;
    std::exit(1);
}

void LLVMGen::warning(std::string msg, long line) {
    std::cout << "\033[0;33mWarning in '"+this->file+"' file at "+std::to_string(line)+" line: "+msg+"\033[0;0m" << std::endl;
}

std::string LLVMGen::mangle(std::string name, bool isFunc, bool isMethod) {
    if(isFunc) {
        if(isMethod) return "_RaveM"+std::to_string(name.size())+name; 
        return "_RaveF"+std::to_string(name.size())+name;
    } return "_RaveG"+name;
}

Type* getTrueStructType(TypeStruct* ts) {
    Type* t = ts->copy();
    while(generator->toReplace.find(t->toString()) != generator->toReplace.end()) t = generator->toReplace[t->toString()];
    return t;
}

std::vector<Type*> getTrueTypeList(Type* t) {
    std::vector<Type*> buffer;
    while(instanceof<TypePointer>(t) || instanceof<TypeArray>(t)) {
        buffer.push_back(t);
        if(instanceof<TypePointer>(t)) t = ((TypePointer*)t)->instance;
        else t = ((TypeArray*)t)->element;
    } buffer.push_back(t);
    return buffer;
}

Type* LLVMGen::setByTypeList(std::vector<Type*> list) {
    std::vector<Type*> buffer = std::vector<Type*>(list);
    if(buffer.size() == 1) {
        if(instanceof<TypeStruct>(buffer[0])) return getTrueStructType((TypeStruct*)list[0]);
        return buffer[0];
    }
    if(instanceof<TypeStruct>(buffer[buffer.size()-1])) buffer[buffer.size()-1] = getTrueStructType((TypeStruct*)buffer[buffer.size()-1]);
    for(int i=buffer.size()-1; i>0; i--) {
        if(instanceof<TypePointer>(buffer[i-1])) ((TypePointer*)buffer[i-1])->instance = buffer[i];
        else if(instanceof<TypeArray>(buffer[i-1])) ((TypeArray*)buffer[i-1])->element = buffer[i];
    }
    return buffer[0];
}

LLVMTypeRef LLVMGen::genType(Type* type, long loc) {
    if(type == nullptr) return LLVMPointerType(LLVMInt8TypeInContext(this->context), 0);
    if(instanceof<TypeAlias>(type)) return nullptr;
    if(instanceof<TypeBasic>(type)) switch(((TypeBasic*)type)->type) {
        case BasicType::Bool: return LLVMInt1TypeInContext(this->context);
        case BasicType::Char: case BasicType::Uchar: return LLVMInt8TypeInContext(this->context);
        case BasicType::Short: case BasicType::Ushort: return LLVMInt16TypeInContext(this->context);
        case BasicType::Int: case BasicType::Uint: return LLVMInt32TypeInContext(this->context);
        case BasicType::Long: case BasicType::Ulong: return LLVMInt64TypeInContext(this->context);
        case BasicType::Cent: case BasicType::Ucent: return LLVMInt128TypeInContext(this->context);
        case BasicType::Float: return LLVMFloatTypeInContext(this->context);
        case BasicType::Double: return LLVMDoubleTypeInContext(this->context);
        default: return nullptr;
    }
    if(instanceof<TypePointer>(type)) {
        if(instanceof<TypeVoid>(((TypePointer*)type)->instance)) return LLVMPointerType(LLVMInt8TypeInContext(this->context),0);
        return LLVMPointerType(this->genType(((TypePointer*)type)->instance, loc),0);
    }
    if(instanceof<TypeArray>(type)) return LLVMArrayType(this->genType(((TypeArray*)type)->element, loc),((TypeArray*)type)->count);
    if(instanceof<TypeStruct>(type)) {
        TypeStruct* s = (TypeStruct*)type;
        if(this->structures.find(s->name) == this->structures.end()) {
            if(this->toReplace.find(s->name) != this->toReplace.end()) return this->genType(this->toReplace[s->name],loc);
            if(s->name.find('<') != std::string::npos) {
                TypeStruct* sCopy = (TypeStruct*)s->copy();
                for(int i=0; i<sCopy->types.size(); i++) sCopy->types[i] = this->setByTypeList(getTrueTypeList(sCopy->types[i]));
                sCopy->updateByTypes();
                if(this->structures.find(sCopy->name) != this->structures.end()) return this->structures[sCopy->name];
                std::string origStruct = sCopy->name.substr(0, sCopy->name.find('<'));
                
                if(AST::structTable.find(origStruct) != AST::structTable.end()) {
                    return AST::structTable[origStruct]->genWithTemplate(sCopy->name.substr(sCopy->name.find('<'), sCopy->name.size()), sCopy->types);
                }
            }
            if(AST::structTable.find(s->name) != AST::structTable.end()) {
                AST::structTable[s->name]->check();
                AST::structTable[s->name]->generate();
                return this->genType(type, loc);
            }
            else if(AST::aliasTypes.find(s->name) != AST::aliasTypes.end()) return this->genType(AST::aliasTypes[s->name], loc);
            this->error("unknown structure '"+s->name+"'!",loc);
        }
        return this->structures[s->name];
    }
    if(instanceof<TypeVoid>(type)) return LLVMVoidTypeInContext(this->context);
    if(instanceof<TypeFunc>(type)) {
        TypeFunc* tf = (TypeFunc*)type;
        if(instanceof<TypeVoid>(tf->main)) {delete tf->main; tf->main = new TypeBasic(BasicType::Char);}
        std::vector<LLVMTypeRef> types;
        for(int i=0; i<tf->args.size(); i++) types.push_back(this->genType(tf->args[i]->type, loc));
        return LLVMPointerType(LLVMFunctionType(this->genType(tf->main,loc),types.data(),types.size(),false),0);
    }
    if(instanceof<TypeFuncArg>(type)) return this->genType(((TypeFuncArg*)type)->type, loc);
    if(instanceof<TypeBuiltin>(type)) {
        /*
        NodeBuiltin nb = new NodeBuiltin(((TypeBuiltin*)type)->name, std::vector<Node*>(((TypeBuiltin*)type)->args), ((TypeBuiltin*)type)->block->copy());
        nb.generate();
        return this->genType(nb->type, loc);
        */
    }
    if(instanceof<TypeConst>(type)) return this->genType(((TypeConst*)type)->instance, loc);
    this->error("undefined type!",loc);
    return nullptr;
}

LLVMValueRef LLVMGen::byIndex(LLVMValueRef value, std::vector<LLVMValueRef> indexes) {
    if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMArrayTypeKind) return byIndex(
        LLVMBuildGEP(generator->builder,value,std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32TypeInContext(generator->context),0,false)}).data(),2,"gep_byIndex"),indexes
    );
    if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(value))) == LLVMArrayTypeKind) value = LLVMBuildPointerCast(
        generator->builder,value,LLVMPointerType(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(value))),0),"ptrc_byIndex"
    );
    if(indexes.size() > 1) {
        LLVMValueRef oneGep = LLVMBuildGEP(generator->builder,value,std::vector<LLVMValueRef>({indexes[0]}).data(),1,"gep2_byIndex");
        return byIndex(oneGep,std::vector<LLVMValueRef>(indexes.begin() + 1, indexes.end()));
    }
    return LLVMBuildGEP(generator->builder,value,indexes.data(),indexes.size(),"gep3_byIndex");
}

void LLVMGen::addAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, long loc, unsigned long value) {
    int id = LLVMGetEnumAttributeKindForName(name.c_str(), name.size());
    if(id == 0) this->error("unknown attribute '"+name+"'!",loc);
    LLVMAddAttributeAtIndex(ptr, index, LLVMCreateEnumAttribute(generator->context, id, value));
}


void LLVMGen::addStrAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, long loc, std::string value) {
    LLVMAttributeRef attr = LLVMCreateStringAttribute(generator->context,name.c_str(),name.size(),value.c_str(),value.size());
    if(attr == nullptr) this->error("unknown attribute '"+name+"'!",loc);
    LLVMAddAttributeAtIndex(ptr, index, attr);
}

Type* lTypeToType(LLVMTypeRef t) {
    if(LLVMGetTypeKind(t) == LLVMIntegerTypeKind) {
        if(t == LLVMInt32TypeInContext(generator->context)) return new TypeBasic(BasicType::Int);
        if(t == LLVMInt64TypeInContext(generator->context)) return new TypeBasic(BasicType::Long);
        if(t == LLVMInt16TypeInContext(generator->context)) return new TypeBasic(BasicType::Short);
        if(t == LLVMInt8TypeInContext(generator->context)) return new TypeBasic(BasicType::Char);
        if(t == LLVMInt1TypeInContext(generator->context)) return new TypeBasic(BasicType::Bool);
        return new TypeBasic(BasicType::Cent);
    }
    else if(LLVMGetTypeKind(t) == LLVMFloatTypeKind) return new TypeBasic((t == LLVMFloatTypeInContext(generator->context)) ? BasicType::Float : BasicType::Double);
    else if(LLVMGetTypeKind(t) == LLVMDoubleTypeKind) return new TypeBasic(BasicType::Double);
    else if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
        if(LLVMGetTypeKind(LLVMGetElementType(t)) == LLVMStructTypeKind) return new TypeStruct(std::string(LLVMGetStructName(LLVMGetElementType(t))));
        return new TypePointer(lTypeToType(LLVMGetElementType(t)));
    }
    else if(LLVMGetTypeKind(t) == LLVMArrayTypeKind) return new TypeArray(LLVMGetArrayLength(t), lTypeToType(LLVMGetElementType(t)));
    else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) return new TypeStruct(std::string(LLVMGetStructName(t)));
    else if(LLVMGetTypeKind(t) == LLVMFunctionTypeKind) {
        std::string sT = std::string(LLVMPrintTypeToString(t));
        sT = sT.substr(0, sT.find_last_of('('));
        return getType(trim(sT));
    }
    generator->error("assert: lTypeToType",-1);
    return nullptr;
}

int LLVMGen::getAlignment(Type* type) {
    if(instanceof<TypeBasic>(type)) {
        switch(((TypeBasic*)type)->type) {
            case BasicType::Bool: case BasicType::Char: case BasicType::Uchar: return 1;
            case BasicType::Short: case BasicType::Ushort: return 2;
            case BasicType::Int: case BasicType::Uint: case BasicType::Float: return 4;
            case BasicType::Long: case BasicType::Ulong: case BasicType::Double: return 8;
            case BasicType::Cent: case BasicType::Ucent: return 16;
            default: return 0;
        }
    }
    else if(instanceof<TypePointer>(type)) return 8;
    else if(instanceof<TypeArray>(type)) return this->getAlignment(((TypeArray*)type)->element);
    else if(instanceof<TypeStruct>(type)) return 8;
    else if(instanceof<TypeFunc>(type)) return this->getAlignment(((TypeFunc*)type)->main);
    else if(instanceof<TypeVoid>(type) || instanceof<TypeAuto>(type)) return 0;
    else if(instanceof<TypeConst>(type)) return this->getAlignment(((TypeConst*)type)->instance);
    return 0;
}

// Scope

Scope::Scope(std::string funcName, std::map<std::string, int> args, std::map<std::string, NodeVar*> argVars) {
    this->funcName = funcName;
    this->args = std::map<std::string, int>(args);
    this->argVars = std::map<std::string, NodeVar*>(argVars);
}

void Scope::remove(std::string name) {
    if(this->localScope.find(name) != this->localScope.end()) {
        this->localScope.erase(name);
        this->localVars.erase(name);
    }
}

LLVMValueRef Scope::get(std::string name, long loc) {
    LLVMValueRef value = nullptr;
    if(AST::aliasTable.find(name) != AST::aliasTable.end()) value = AST::aliasTable[name]->generate();
    else if(this->localScope.find(name) != this->localScope.end()) value = this->localScope[name];
    else if(generator->globals.find(name) != generator->globals.end()) value = generator->globals[name];
    else if(this->has(name) && this->args.find(name) == this->args.end() && generator->functions.find(name) == generator->functions.end()) {
        Type* varType = this->getVar(name,loc)->type;
        if(instanceof<TypePointer>(varType)) varType = ((TypePointer*)varType)->instance;
        /*if(AST::structTable.find(((TypeStruct*)varType)->name) != AST::structTable.end()) {
            TODO
        }*/
        generator->error("TODO",loc);
        return nullptr;
    }
    else if(generator->functions.find(this->funcName) != generator->functions.end()) {
        if(this->args.find(name) == this->args.end()) {
            if(generator->functions.find(name) == generator->functions.end()) generator->error("undefined identifier '"+name+"'!", loc);
            return generator->functions[name];
        }
        return LLVMGetParam(generator->functions[this->funcName], this->args[name]);
    }
    if(value == nullptr) {
        generator->error("undefined variable '"+name+"'!", loc);
        return nullptr;
    }
    if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind) value = LLVMBuildLoad(generator->builder, value, "scopeGetLoad");
    return value;
}

LLVMValueRef Scope::getWithoutLoad(std::string name, long loc) {
    if(this->localScope.find(name) != this->localScope.end()) return this->localScope[name];
    if(generator->globals.find(name) != generator->globals.end()) return generator->globals[name];
    if(this->has("this") && this->args.find(name) == this->args.end()) {
        // TODO:
        /*Type _t = currScope.getVar("this",loc).t;
        if(TypePointer tp = _t.instanceof!TypePointer) _t = tp.instance;
        TypeStruct ts = _t.instanceof!TypeStruct;

        if(ts.name.into(StructTable)) {
            if(cast(immutable)[ts.name,n].into(structsNumbers)) {
                return new NodeGet(new NodeIden("this",loc),n,true,loc).generate();
            }
        }*/
        return nullptr;
    }
    if(this->args.find(name) == this->args.end()) generator->error("undefined identifier '"+name+"'!",loc);
    return LLVMGetParam(generator->functions[this->funcName], this->args[name]);
}

bool Scope::has(std::string name) {
    return AST::aliasTable.find(name) != AST::aliasTable.end() ||
        this->localScope.find(name) != this->localScope.end() ||
        generator->globals.find(name) != generator->globals.end() ||
        this->args.find(name) != this->args.end();
}

NodeVar* Scope::getVar(std::string name, long loc) {
    if(this->localVars.find(name) != this->localVars.end()) return this->localVars[name];
    if(AST::varTable.find(name) != AST::varTable.end()) return AST::varTable[name];
    if(this->argVars.find(name) != this->argVars.end()) return this->argVars[name];
    if(this->has("this")) {
        // TODO:
        /*Type _t = currScope.getVar("this",line).t;
        if(TypePointer tp = _t.instanceof!TypePointer) _t = tp.instance;
        TypeStruct ts = _t.instanceof!TypeStruct;
        if(ts.name.into(StructTable)) {
            if(cast(immutable)[ts.name,n].into(structsNumbers)) {
                return structsNumbers[cast(immutable)[ts.name,n]].var;
            }
        }*/
    }
    generator->error("undefined variable '"+name+"'!",loc);
    return nullptr;
}

void Scope::hasChanged(std::string name) {
    if(this->localVars.find(name) != this->localVars.end()) ((NodeVar*)this->localVars[name])->isChanged = true;
    if(AST::varTable.find(name) != AST::varTable.end()) AST::varTable[name]->isChanged = true;
    if(this->argVars.find(name) != this->argVars.end()) ((NodeVar*)this->argVars[name])->isChanged = true;
}

std::string typeToString(LLVMTypeRef type) {return std::string(LLVMPrintTypeToString(type));}