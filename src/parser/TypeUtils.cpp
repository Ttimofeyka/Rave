/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/TypeUtils.hpp"
#include "../include/parser/nodes/NodeCall.hpp"
#include "../include/parser/nodes/NodeIden.hpp"
#include "../include/parser/nodes/NodeType.hpp"
#include "../include/parser/nodes/NodeInt.hpp"
#include "../include/parser/Types.hpp"
#include "../include/llvm.hpp"

TypeFunc* callToTFunc(NodeCall* call) {
    std::vector<TypeFuncArg*> argTypes;
    for (Node* nd: call->args) {
        if (instanceof<NodeCall>(nd)) argTypes.push_back(new TypeFuncArg(callToTFunc((NodeCall*)nd), ""));
        else if (instanceof<NodeIden>(nd)) argTypes.push_back(new TypeFuncArg(getTypeByName(((NodeIden*)nd)->name), ""));
        else if (instanceof<NodeType>(nd)) argTypes.push_back(new TypeFuncArg(((NodeType*)nd)->type, ""));
    }
    return new TypeFunc(getTypeByName(((NodeIden*)call->func)->name), argTypes, false);
}

std::vector<Type*> parametersToTypes(std::vector<RaveValue> params) {
    if (params.empty()) return {};
    std::vector<Type*> types;
    for (size_t i=0; i<params.size(); i++) types.push_back(params[i].type);
    return types;
}

std::string typeToString(Type* arg) {
    if (instanceof<TypeBasic>(arg)) {
        switch (((TypeBasic*)arg)->type) {
            case BasicType::Half: return "hf";
            case BasicType::Bhalf: return "bf";
            case BasicType::Float: return "f";
            case BasicType::Double: return "d";
            case BasicType::Int: return "i";
            case BasicType::Uint: return "ui";
            case BasicType::Cent: return "t";
            case BasicType::Ucent: return "ut";
            case BasicType::Char: return "c";
            case BasicType::Uchar: return "uc";
            case BasicType::Long: return "l";
            case BasicType::Ulong: return "ul";
            case BasicType::Short: return "h";
            case BasicType::Ushort: return "uh";
            case BasicType::Real: return "r";
            default: return "b";
        }
    }
    else if (instanceof<TypePointer>(arg)) {
        if (instanceof<TypeStruct>(((TypePointer*)arg)->instance)) {
            TypeStruct* ts = (TypeStruct*)(((TypePointer*)arg)->instance);
            Type* t = ts;
            while (generator->toReplace.find(t->toString()) != generator->toReplace.end()) t = generator->toReplace[t->toString()];
            if (!instanceof<TypeStruct>(t)) return typeToString(new TypePointer(t));
            ts = (TypeStruct*)t;
            if (ts->name.find('<') == std::string::npos) return "s-" + ts->name;
            else return "s-" + ts->name.substr(0, ts->name.find('<'));
        }
        else {
            std::string buffer = "p";
            Type* element = ((TypePointer*)arg)->instance;

            while (!instanceof<TypeBasic>(element) && !instanceof<TypeVoid>(element) && !instanceof<TypeFunc>(element) && !instanceof<TypeStruct>(element)) {
                if (instanceof<TypeConst>(element)) {element = ((TypeConst*)element)->instance; continue;}

                if (instanceof<TypeArray>(element)) {
                    TypeArray* tarray = (TypeArray*)element;
                    if (!instanceof<TypeBasic>(tarray->element) && !instanceof<TypeVoid>(tarray->element) &&
                       !instanceof<TypeFunc>(tarray->element) && !instanceof<TypeStruct>(tarray->element)) buffer += "a";
                    element = ((TypeArray*)element)->element;
                }
                else if (instanceof<TypePointer>(element)) {
                    buffer += "p";
                    element = ((TypePointer*)element)->instance;
                }
            }

            buffer += (instanceof<TypeVoid>(element) ? "c" : typeToString(element));
            return buffer;
        }
    }
    else if (instanceof<TypeArray>(arg)) {
        std::string buffer = "a" + std::to_string(((NodeInt*)((TypeArray*)arg)->count->comptime())->value.to_int());
        Type* element = ((TypeArray*)arg)->element;

        while (element != nullptr && !instanceof<TypeBasic>(element) && !instanceof<TypeFunc>(element) && !instanceof<TypeStruct>(element)) {
            if (instanceof<TypeConst>(element)) {element = ((TypeConst*)element)->instance; continue;}

            if (instanceof<TypeArray>(element)) {
                buffer += "a";
                if (!instanceof<TypeVoid>(((TypeArray*)element)->element)) element = ((TypeArray*)element)->element;
            }
            else if (instanceof<TypePointer>(element)) {
                buffer += "p";
                if (!instanceof<TypeVoid>(((TypePointer*)element)->instance)) element = ((TypePointer*)element)->instance;
            }
        }

        if (element != nullptr) buffer += typeToString(element);
        return buffer;
    }
    else if (instanceof<TypeStruct>(arg)) {
        Type* ty = arg;
        Types::replaceTemplates(&ty);

        if (!instanceof<TypeStruct>(ty)) return typeToString(ty);

        return "s-" + ty->toString();
    }
    else if (instanceof<TypeVector>(arg)) return "v" + typeToString(((TypeVector*)arg)->mainType) + std::to_string(((TypeVector*)arg)->count);
    else if (instanceof<TypeFunc>(arg)) return "func";
    else if (instanceof<TypeConst>(arg)) return typeToString(((TypeConst*)arg)->instance);
    return "";
}

std::string typesToString(std::vector<Type*>& args) {
    std::string data = "[";
    for (size_t i=0; i<args.size(); i++) data += "_" + typeToString(args[i]);
    return data + "]";
}

std::string typesToString(std::vector<FuncArgSet>& args) {
    std::string data = "[";
    for (size_t i=0; i<args.size(); i++) data += "_" + typeToString(args[i].type);
    return data + "]";
}
