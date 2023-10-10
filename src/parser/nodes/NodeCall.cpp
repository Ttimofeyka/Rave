/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/lexer/lexer.hpp"

NodeCall::NodeCall(long loc, Node* func, std::vector<Node*> args) {
    this->loc = loc;
    this->func = func;
    this->args = std::vector<Node*>(args);
}

Type* NodeCall::getType() {
    if(instanceof<NodeIden>(this->func)) {
        if(AST::funcTable.find((((NodeIden*)this->func)->name+typesToString(this->getTypes()))) != AST::funcTable.end()) return AST::funcTable[(((NodeIden*)this->func)->name+typesToString(this->getTypes()))]->getType();
    }
    return this->func->getType();
}

std::vector<Type*> NodeCall::getTypes() {
    std::vector<Type*> arr;
    for(int i=0; i<this->args.size(); i++) arr.push_back(this->args[i]->getType());
    return arr;
}

std::vector<LLVMValueRef> NodeCall::correctByLLVM(std::vector<LLVMValueRef> values) {
    if(this->calledFunc == nullptr || values.empty()) return values;
    std::vector<LLVMValueRef> corrected = std::vector<LLVMValueRef>(values);
    std::vector<LLVMTypeRef> _types = this->calledFunc->genTypes;
    if(_types.empty()) return values;

    for(int i=0; i<corrected.size(); i++) {
        if(LLVMTypeOf(corrected[i]) != _types[i+_offset]) {
            if(LLVMTypeOf(corrected[i]) == LLVMPointerType(_types[i+_offset],0)) corrected[i] = LLVMBuildLoad(generator->builder, corrected[i], "correctByLLVM_load");
            else if(_types[i] == LLVMPointerType(LLVMTypeOf(corrected[i]),0)) {
                LLVMValueRef v = LLVMBuildAlloca(generator->builder ,LLVMTypeOf(corrected[i]), "correctByLLVM_temp");
                LLVMBuildStore(generator->builder,corrected[i],v);
                corrected[i] = v;
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(corrected[i])) == LLVMIntegerTypeKind 
                 && LLVMGetTypeKind(_types[i+_offset]) == LLVMIntegerTypeKind) corrected[i] = LLVMBuildIntCast(generator->builder, corrected[i], _types[i], "correctByLLVM_intc");
            else if(LLVMGetTypeKind(LLVMTypeOf(corrected[i])) == LLVMFloatTypeKind 
                 && LLVMGetTypeKind(_types[i+_offset]) == LLVMDoubleTypeKind) corrected[i] = LLVMBuildFPCast(generator->builder, corrected[i], _types[i], "correctByLLVM_floatc");
            else if(LLVMGetTypeKind(LLVMTypeOf(corrected[i])) == LLVMDoubleTypeKind
                 && LLVMGetTypeKind(_types[i+_offset]) == LLVMFloatTypeKind) corrected[i] = LLVMBuildFPCast(generator->builder,corrected[i], _types[i], "correctByLLVM_floatc");
        }
    }
    return corrected;
}

std::vector<LLVMValueRef> NodeCall::getParameters(bool isVararg, std::vector<FuncArgSet> fas) {
    std::vector<LLVMValueRef> params;

    if(isVararg) {
        for(int i=0; i<args.size(); i++) params.push_back(args[i]->generate());
        return params;
    }

    if(!fas.empty()) {
        int offset = 0;
        if(fas[0].name == "this") {
            offset = 1;
            if(args.size() >= 2) {
                if(instanceof<NodeIden>(args[0]) && instanceof<NodeIden>(args[1])) {
                    if(((NodeIden*)args[0])->name == ((NodeIden*)args[1])->name && fas.size() >= 2 && fas[0].name != fas[1].name) args = std::vector<Node*>(args.begin()+1, args.end());
                }
            }
        }

        for(int i = 0; i<fas.size(); i++) {
            if(i == args.size()) break;
            LLVMValueRef arg = args[i]->generate();

            if(arg == nullptr) continue;

            if(instanceof<TypePointer>(fas[i].type)) {
                TypePointer* tp = (TypePointer*)fas[i].type;
                if(instanceof<TypeVoid>(tp->instance)) {
                    if(LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMPointerTypeKind) arg = (new NodeCast(new TypePointer(new TypeVoid()), args[i], this->loc))->generate();
                }
                if(instanceof<TypeStruct>(tp->instance) && LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMPointerTypeKind) {
                    if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(arg))) == LLVMPointerTypeKind) arg = LLVMBuildLoad(generator->builder, arg, "NodeCall_getParameters_load");
                }
                else if(instanceof<TypeStruct>(tp->instance) && LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind) {
                    if(LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind) {
                        if(instanceof<NodeIden>(args[i])) {
                            ((NodeIden*)args[i])->isMustBePtr = true;
                            arg = args[i]->generate();
                        }
                        else if(instanceof<NodeIndex>(args[i])) {
                            ((NodeIndex*)args[i])->isMustBePtr = true;
                            arg = args[i]->generate();
                        }
                        else if(instanceof<NodeGet>(args[i])) {
                            ((NodeGet*)args[i])->isMustBePtr = true;
                            arg = args[i]->generate();
                        }
                    }
                }

                if(LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind) {
                    if(instanceof<NodeIden>(args[i])) {((NodeIden*)args[i])->isMustBePtr = true; arg = args[i]->generate();}
                    else if(instanceof<NodeGet>(args[i])) {((NodeGet*)args[i])->isMustBePtr = true; arg = args[i]->generate();}
                    else if(instanceof<NodeIndex>(args[i])) {((NodeIndex*)args[i])->isMustBePtr = true; arg = args[i]->generate();}

                    if(LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind && (fas[i].name != "this" || LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMStructTypeKind)) {
                        LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(arg), "NodeCall_getParameters_localcopy");
                        LLVMBuildStore(generator->builder, arg, temp);
                        arg = temp;
                    }
                }
            }
            params.push_back(arg);
        }
    }
    else for(int i=0; i<args.size(); i++) {
        if(instanceof<NodeType>(args[i])) continue;
        LLVMValueRef arg = args[i]->generate();
        params.push_back(arg);
    }

    return correctByLLVM(params);
}

LLVMValueRef NodeCall::generate() {
    if(instanceof<NodeIden>(func)) {
    NodeIden* f = ((NodeIden*)func);
    if(AST::funcTable.find(f->name) == AST::funcTable.end() &&
       AST::funcTable.find(f->name+std::to_string(this->args.size())) == AST::funcTable.end() &&
       AST::funcTable.find(f->name+typesToString(this->getTypes())) == AST::funcTable.end()) {
        if(currScope->has(f->name)) {
            TypeFunc* tf = (TypeFunc*)(currScope->getVar(f->name, this->loc))->type;
            return LLVMBuildCall(
                generator->builder,
                currScope->get(f->name, this->loc),
                this->getParameters(false).data(),
                this->args.size(),
                (instanceof<TypeVoid>(tf->main) ? "" : ("CallVar"+f->name)).c_str()
            );
        }
        if(generator->toReplace.find(f->name) != generator->toReplace.end()) {
            f->name = generator->toReplace[f->name]->toString();
            return this->generate();
        }
        if(f->name.find('<') != std::string::npos) {
            // TODO
            /*if(!f.name[0..f.name.find('<')].into(AST::funcTable)) {
                Lexer l = new Lexer(f.name,1);
                Parser p = new Parser(l.getTokens(),1,"(builtin)");
                TypeStruct _t = p.parseType().instanceof!TypeStruct;

                bool _replaced = false;
                for(int i=0; i<_t.types.size(); i++) {
                    if(_t.types[i].toString().into(Generator.toReplace)) {
                        _t.types[i] = Generator.toReplace[_t.types[i].toString()];
                        _replaced = true;
                    }
                }

                if(_replaced) {
                    _t.updateByTypes();
                    NodeCall nc = new NodeCall(loc,new NodeIden(_t.name,f.loc),args.dup);
                    if(_t.name.into(AST::funcTable)) {
                        return nc.generate();
                    }
                    else if(_t.name[0.._t.name.find('<')].into(AST::funcTable)) {
                        AST::funcTable[_t.name[0.._t.name.find('<')]].generateWithTemplate(_t.types,_t.name);
                        return nc.generate();
                    }
                }
                else if(f.name[0..f.name.find('<')].into(StructTable)) {
                    StructTable[f.name[0..f.name.find('<')]].generateWithTemplate(f.name[f.name.find('<')..$],_t.types.dup);
                    return generate();
                }
                generator->error(loc,"Unknown mixin or function '"+f.name+"'!");
            }

            // Template function
            Lexer l = new Lexer(f.name,1);
            Parser p = new Parser(l.getTokens(),1,"(builtin)");
            AST::funcTable[f.name[0..f.name.find('<')]].generateWithTemplate(p.parseType().instanceof!TypeStruct.types,f.name);
            return generate();*/
        }
        for(auto const& x : AST::funcTable) {
            std::cout << x.first << std::endl;
        }
        generator->error("unknown function '"+f->name+"'!", this->loc);
        return nullptr;
    }
    LLVMValueRef value;
    std::string rname = f->name;
    std::vector<LLVMValueRef> params;
    std::vector<Type*> types = this->getTypes();
    std::string sTypes = typesToString(types);

    if(AST::funcTable.find(f->name+sTypes) != AST::funcTable.end()) {
        rname += sTypes;
        if(generator->functions.find(rname) == generator->functions.end()) {
            if(generator->functions.find(f->name) != generator->functions.end()) {
                this->calledFunc = AST::funcTable[f->name];
                rname = f->name;
            }
            else {
                generator->error("function '"+rname+"' does not exist!", this->loc);
                return nullptr;
            }
        }
        else this->calledFunc = AST::funcTable[rname];
    }
    else {
        if(generator->functions.find(rname) == generator->functions.end()) {
            if(AST::funcTable.find(rname) == AST::funcTable.end()) {
                generator->error("Function '"+rname+"' does not exist!", this->loc);
                return nullptr;
            }
            if(AST::funcTable[rname]->isCtargs) {
                if(AST::funcTable.find(rname+sTypes) == AST::funcTable.end()) {
                    std::vector<LLVMTypeRef> lTypes;
                    for(int i=0; i<types.size(); i++) lTypes.push_back(generator->genType(types[i], this->loc));
                    rname = AST::funcTable[rname]->generateWithCtargs(lTypes);
                }
                else rname = rname+sTypes;
            }
        }
        calledFunc = AST::funcTable[rname];
        params = getParameters(AST::funcTable[rname]->isVararg, AST::funcTable[rname]->args);
    }
    std::string name = "CallFunc"+f->name;
    if(instanceof<TypeVoid>(AST::funcTable[rname]->type)) name = "";
    if(AST::funcTable[rname]->args.size() != args.size() && AST::funcTable[rname]->isVararg != true) {
        generator->error(
            "the number of arguments in the call of string "+rname+" ("+std::to_string(args.size())+") does not match the signature ("+std::to_string(AST::funcTable[rname]->args.size())+")!",
            this->loc
        );
        return nullptr;
    }
    if(generator->functions.find(rname) == generator->functions.end()) {
        generator->error("function '"+rname+"' does not exist!", this->loc);
        return nullptr;
    }
    if(std::string(LLVMPrintValueToString(generator->functions[rname])).find("llvm.") != std::string::npos) {
        LLVMValueRef _v = LLVMBuildAlloca(generator->builder, LLVMInt1TypeInContext(generator->context), "fix");
        if(generator->settings.optLevel > 0) LLVMBuildCall(generator->builder, generator->functions["std::dontuse::_void"], std::vector<LLVMValueRef>({_v}).data(), 1, "");
        // This is necessary to solve a bug with LLVM-11 and higher
    }
    if(args.size() != params.size()) params = this->getParameters(AST::funcTable[rname]->isVararg, AST::funcTable[rname]->args);
    value = LLVMBuildCall(
        generator->builder,
        generator->functions[rname],
        params.data(),
        args.size(),
        name.c_str()
    );
    if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
    if(currScope->inTry) {
        if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMStructTypeKind) {
            if((std::string(LLVMGetStructName(LLVMTypeOf(value)))).find("std::error<") != -1) {
                (new NodeCall(this->loc, new NodeGet(new NodeDone(value), "catch", true, this->loc), {}))->generate();
                value = (new NodeGet(new NodeDone(value), "result", false, this->loc))->generate();
            }
        }
    }
    return value;
    }
    else if(instanceof<NodeGet>(this->func)) {
        NodeGet* g = (NodeGet*)this->func;
        LLVMValueRef value;
        if(instanceof<NodeIden>(g->base)) {
            NodeIden* i = (NodeIden*)g->base;
            TypeStruct* s = nullptr;
            if(instanceof<TypePointer>(currScope->getVar(i->name, this->loc)->type)) s = (TypeStruct*)(((TypePointer*)currScope->getVar(i->name, this->loc)->type)->instance);
            else s = (TypeStruct*)currScope->getVar(i->name, this->loc)->type;

            if(s == nullptr) {
                // Template
                LLVMValueRef v = currScope->getWithoutLoad(i->name, this->loc);

                if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) s = new TypeStruct(std::string(LLVMGetStructName(LLVMTypeOf(v))));
                else if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMPointerTypeKind) {
                    if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(v))) == LLVMStructTypeKind) s = new TypeStruct(std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(v)))));
                    else if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(v))) == LLVMPointerTypeKind) s = new TypeStruct(std::string(LLVMGetStructName(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(v))))));
                }
            }
            this->args.insert(this->args.begin(), new NodeIden(i->name, this->loc));

            LLVMValueRef _toCall = nullptr;
            NodeVar* v = nullptr;
            TypeFunc* t = nullptr;

            if(s == nullptr || AST::funcTable.find(g->field) != AST::funcTable.end()) {
                // Just call function as method of value
                if(args.size() != AST::funcTable[g->field]->args.size()) {
                    generator->error(
                        "The number of arguments in the call of function '"+AST::funcTable[g->field]->name+"' ("+std::to_string(args.size())+") does not match the signature ("+std::to_string(AST::funcTable[g->field]->args.size())+")!",
                        this->loc
                    );
                    return nullptr;
                }

                this->calledFunc = AST::funcTable[g->field];
                std::vector<LLVMValueRef> params = getParameters(AST::funcTable[g->field]->isVararg, AST::funcTable[g->field]->args);
                value = LLVMBuildCall(
                    generator->builder,
                    generator->functions[AST::funcTable[g->field]->name],
                    params.data(),
                    params.size(),
                    (instanceof<TypeVoid>(AST::funcTable[g->field]->type) ? "" : "call")
                );
                if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            }

            std::vector<LLVMValueRef> params;
            if(generator->toReplace.find(s->name) != generator->toReplace.end()) s = (TypeStruct*)generator->toReplace[s->name];
            if(AST::structTable.find(s->name) == AST::structTable.end() && s->name.find('<') != std::string::npos) {
                if(generator->toReplace.find(s->name) != generator->toReplace.end()) s = (TypeStruct*)generator->toReplace[s->name];
                else {
                    Lexer* lexer = new Lexer(s->name, 1);
                    Parser* parser = new Parser(lexer->tokens, "(builtin_sname)");
                    TypeStruct* ts = (TypeStruct*)parser->parseType();
                    for(int j=0; j<ts->types.size(); j++) {
                        while(generator->toReplace.find(ts->types[j]->toString()) != generator->toReplace.end()) ts->types[j] = generator->toReplace[ts->types[j]->toString()];
                    }
                    ts->updateByTypes();
                    s = ts;
                }
            }

            if(AST::methodTable.find(std::pair<std::string, std::string>(s->name, g->field)) == AST::methodTable.end()) {
                if(AST::structsNumbers.find(std::pair<std::string, std::string>(s->name, g->field)) == AST::structsNumbers.end()) {
                    std::vector<Type*> pregenerate = this->getTypes();
                    std::string n = "";
                    if(instanceof<TypePointer>(pregenerate[0])) n = typeToString(generator->genType(pregenerate[0], this->loc)).substr(1);
                    else n = std::string(LLVMGetStructName(generator->genType(pregenerate[0], this->loc)));

                    if(g->field.find('<') != std::string::npos) {
                        std::string newName = g->field.substr(0, g->field.find('<'));
                        Lexer* lexer = new Lexer(g->field, 1);
                        Parser* parser = new Parser(lexer->tokens, "(builtin_im)");
                        if(AST::funcTable.find(("{"+n+"*}"+newName)) != AST::funcTable.end()) {
                            TypeStruct* ts = (TypeStruct*)parser->parseType();
                            AST::funcTable["{"+n+"*}"+newName]->generateWithTemplate(ts->types, ts->name);
                            return (new NodeCall(this->loc, new NodeIden(("{"+n+"*}"+ts->name), this->loc), args))->generate();
                        }
                        else if(AST::funcTable.find(("{"+n+"}"+newName)) != AST::funcTable.end()) {
                            TypeStruct* ts = (TypeStruct*)parser->parseType();
                            AST::funcTable["{"+n+"}"+newName]->generateWithTemplate(ts->types, ts->name);
                            return (new NodeCall(this->loc, new NodeIden(("{"+n+"}"+ts->name), this->loc), args))->generate();
                        }
                        generator->error("assert into NodeCall!", this->loc);
                        return nullptr;
                    }

                    if(AST::funcTable.find(("{"+n+"}"+g->field)) != AST::funcTable.end() || AST::funcTable.find(("{"+n+"*}"+g->field)) != AST::funcTable.end()) {
                        // Internal method
                        if(AST::funcTable.find(("{"+n+"*}"+g->field)) != AST::funcTable.end()) return (new NodeCall(this->loc, new NodeIden(("{"+n+"*}"+g->field), this->loc),args))->generate();
                        return (new NodeCall(this->loc, new NodeIden(("{"+n+"}"+g->field), this->loc), args))->generate();
                    }
                    generator->error("structure '"+s->name+"' does not contain method '"+g->field+"'!", this->loc);
                    return nullptr;
                }
                v = (NodeVar*)AST::structTable[s->name]->elements[AST::structsNumbers[std::pair<std::string, std::string>(s->name,g->field)].number];
                if(instanceof<TypePointer>(v->type)) t = (TypeFunc*)(((TypePointer*)v->type)->instance);
                else t = (TypeFunc*)v->type;

                std::vector<FuncArgSet> _args;
                for(int z=0; z<t->args.size(); z++) _args.push_back(FuncArgSet{.name = t->args[z]->name, .type = t->args[z]->type});

                if(args.size() < 2) args.clear();
                else args = std::vector<Node*>(args.begin()+1, args.end());

                params = this->getParameters(false, _args);
                g->isMustBePtr = true;
                _toCall = LLVMBuildLoad(
                    generator->builder, 
                    generator->byIndex(g->generate(),
                        std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32TypeInContext(generator->context), AST::structsNumbers[std::pair<std::string, std::string>(s->name, g->field)].number, false)})
                    ),
                    "NodeCall_toCallLoad"
                );
            }
            else {
                calledFunc = AST::methodTable[std::pair<std::string, std::string>(s->name, g->field)];
                params = getParameters(
                    AST::methodTable[std::pair<std::string, std::string>(s->name, g->field)]->isVararg,
                    AST::methodTable[std::pair<std::string, std::string>(s->name,g->field)]->args
                );
            }

            if(_toCall != nullptr) {
                value = LLVMBuildCall(
                    generator->builder,
                    _toCall,
                    params.data(),
                    params.size(),
                    (instanceof<TypeVoid>(t->main) ? "" : "call")
                );
                if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
                return value;
            }
            std::string newName = typesToString(parametersToTypes(params));
            if(AST::methodTable.find(std::pair<std::string, std::string>(s->name, g->field+newName)) != AST::methodTable.end()) {
                calledFunc = AST::methodTable[std::pair<std::string, std::string>(s->name, g->field+newName)];
                params = getParameters(
                    AST::methodTable[std::pair<std::string, std::string>(s->name, g->field+newName)]->isVararg,
                    AST::methodTable[std::pair<std::string, std::string>(s->name, g->field+newName)]->args
                );
                value = LLVMBuildCall(
                    generator->builder,
                    generator->functions[AST::methodTable[std::pair<std::string, std::string>(s->name,g->field+newName)]->name],
                    params.data(),
                    params.size(),
                    (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(s->name, g->field+newName)]->type) ? "" : g->field).c_str()
                );
                if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
                return value;
            }
            value = LLVMBuildCall(
                generator->builder,
                generator->functions[AST::methodTable[std::pair<std::string, std::string>(s->name, g->field)]->name],
                params.data(),
                params.size(),
                (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(s->name, g->field)]->type) ? "" : g->field).c_str()
            );
            if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            return value;
        }
        else if(instanceof<NodeIndex>(g->base)) {
            NodeIndex* index = ((NodeIndex*)g->base);
            auto val = index->generate();
            std::string sname = std::string(LLVMPrintTypeToString(LLVMTypeOf(val)));
            sname = sname.substr(1, sname.size()-1);

            calledFunc = AST::methodTable[std::pair<std::string, std::string>(sname, g->field)];
            std::vector<LLVMValueRef> params = getParameters(
                AST::methodTable[std::pair<std::string, std::string>(sname,g->field)]->isVararg,
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->args
            );

            params.insert(params.begin(), val);
            value = LLVMBuildCall(
                generator->builder,
                generator->functions[AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->name],
                params.data(),
                params.size(),
                (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->type) ? "" : g->field).c_str()
            );
            if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            return value;
        }
        else if(instanceof<NodeGet>(g->base)) {
            NodeGet* ng = (NodeGet*)g->base;
            ng->isMustBePtr = true;
            auto val = ng->generate();
            std::string sname = "";
            if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMStructTypeKind) sname = std::string(LLVMGetStructName(LLVMTypeOf(val)));
            else sname = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(val))));

            calledFunc = AST::methodTable[std::pair<std::string, std::string>(sname, g->field)];
            _offset = 1;
            std::vector<LLVMValueRef> params = this->getParameters(
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->isVararg,
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->args
            );
            _offset = 0;

            params.insert(params.begin(), val);
            value = LLVMBuildCall(
                generator->builder,
                generator->functions[AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->name],
                params.data(),
                params.size(),
                (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->type) ? "" : g->field).c_str()
            );
            if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            return value;
        }
        else if(instanceof<NodeCast>(g->base)) {
            NodeCast* nc = (NodeCast*)g->base;
            LLVMValueRef v = nc->generate();
            if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) {
                LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(v), "NodeCall_temp");
                LLVMBuildStore(generator->builder, v, temp);
                v = temp;
            }

            if(!instanceof<TypeStruct>(nc->type)) {
                generator->error("type '"+nc->type->toString()+"' is not a structure!", this->loc);
                return nullptr;
            }
            std::string sname = ((TypeStruct*)nc->type)->name;
            calledFunc = AST::methodTable[std::pair<std::string, std::string>(sname, g->field)];
            _offset = 1;
            std::vector<LLVMValueRef> params = getParameters(
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->isVararg,
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->args
            );
            _offset = 0;

            params.insert(params.begin(), v);
            value = LLVMBuildCall(
                generator->builder,
                generator->functions[AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->name],
                params.data(),
                params.size(),
                (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->type) ? "" : g->field).c_str()
            );
            if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            return value;
        }
        else if(instanceof<NodeBuiltin>(g->base)) {
            NodeBuiltin* nb = (NodeBuiltin*)g->base;
            LLVMValueRef v = nb->generate();
            std::string sname = "";

            if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) {
                sname = std::string(LLVMGetStructName(LLVMTypeOf(v)));
                LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(v), "NodeCall_temp");
                LLVMBuildStore(generator->builder, v, temp);
                v = temp;
            }
            else sname = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(v))));
            if(AST::structTable.find(sname) == AST::structTable.end()) {
                generator->error("type '"+sname+"' is not a structure!", this->loc);
            }
            calledFunc = AST::methodTable[std::pair<std::string, std::string>(sname, g->field)];
            _offset = 1;
            std::vector<LLVMValueRef> params = getParameters(
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->isVararg,
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->args
            );
            _offset = 0;

            params.insert(params.begin(), v);
            value = LLVMBuildCall(
                generator->builder,
                generator->functions[AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->name],
                params.data(),
                params.size(),
                (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->type) ? "" : g->field).c_str()
            );
            if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            return value;
        }
        else if(instanceof<NodeDone>(g->base)) {
            NodeDone* nd = (NodeDone*)g->base;
            LLVMValueRef v = nd->generate();
            std::string sname = "";

            if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) {
                sname = std::string(LLVMGetStructName(LLVMTypeOf(v)));
                LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(v), "NodeCall_inverted");
                LLVMBuildStore(generator->builder,v,temp);
                v = temp;
            }

            if(AST::structTable.find(sname) == AST::structTable.end()) {
                generator->error("type '"+sname+"' is not a structure!", loc);
                return nullptr;
            }
            calledFunc = AST::methodTable[std::pair<std::string, std::string>(sname, g->field)];
            _offset = 1;
            std::vector<LLVMValueRef> params = getParameters(
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->isVararg,
                AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->args
            );
            _offset = 0;

            params.insert(params.begin(), v);
            value = LLVMBuildCall(
                generator->builder,
                generator->functions[AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->name],
                params.data(),
                params.size(),
                (instanceof<TypeVoid>(AST::methodTable[std::pair<std::string, std::string>(sname, g->field)]->type) ? "" : g->field).c_str()
            );
            if(this->isInverted) value = LLVMBuildNot(generator->builder, value, "NodeCall_inverted");
            return value;
        }
        else {
            generator->error("a call of this kind is temporarily unavailable!", this->loc);
            return nullptr;
        }
    }
    else if(instanceof<NodeUnary>(this->func)) {
        NodeCall* nc = new NodeCall(this->loc, ((NodeUnary*)this->func)->base, this->args);
        if(((NodeUnary*)this->func)->type == TokType::Ne) nc->isInverted = true;
        return nc->generate();
    }
    generator->error("a call of this kind is temporarily unavailable!", this->loc);
    return nullptr;
}

void NodeCall::check() {this->isChecked = true;}
Node* NodeCall::comptime() {return this;}
Node* NodeCall::copy() {return new NodeCall(this->loc, this->func, this->args);}