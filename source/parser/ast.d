/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module parser.ast;

import std.array;
import std.conv : to;
import std.stdio;
import std.string;
import core.stdc.stdlib : exit;
import lexer.tokens;
import parser.types;
import parser.parser;
import std.algorithm : map;
import std.algorithm : canFind;
import llvm;
import std.file;
import std.path;
import lexer.lexer : Lexer;
import parser.parser : Parser;
import app : CompOpts;
import std.bigint;
import app : ver;
import std.random : uniform;

Node[string] AliasTable;
NodeFunc[string] FuncTable;
NodeVar[string] VarTable;
NodeStruct[string] StructTable;
NodeMacro[string] MacroTable;
bool[string] ConstVars;
NodeFunc[string[]] MethodTable;
NodeLambda[string] LambdaTable;
NodeMixin[string] MixinTable;

string[] _libraries;
string[string] NeededFunctions;
string[] _importedFiles;
string[] _addToImport;
string ASTMainFile;

Node[][string] _parsed;

int countOf(string s, char c) {
    pragma(inline,true);
    int a = 0;
    for(int i=0; i<s.length; i++) {
        if(s[i] == c) a += 1;
    }
    return a;
}

bool into(T, TT)(T t, TT tt) {
    pragma(inline,true);
    return !(t !in tt);
}

void checkError(int loc,string msg) {
    pragma(inline,true);
        writeln("\033[0;31mError on "~to!string(loc)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
}

struct LoopReturn {
    NodeRet ret;
    int loopId;
}

struct Loop {
    bool isActive;
    LLVMBasicBlockRef start;
    LLVMBasicBlockRef end;
    bool hasEnd;
    bool isIf;
    LoopReturn[] rets;
}

Type llvmTypeToType(LLVMTypeRef t) {
        if(LLVMGetTypeKind(t) == LLVMIntegerTypeKind) {
            if(t == LLVMInt32TypeInContext(Generator.Context)) return new TypeBasic("int");
            if(t == LLVMInt64TypeInContext(Generator.Context)) return new TypeBasic("long");
            if(t == LLVMInt16TypeInContext(Generator.Context)) return new TypeBasic("short");
            if(t == LLVMInt8TypeInContext(Generator.Context)) return new TypeBasic("char");
            if(t == LLVMInt1TypeInContext(Generator.Context)) return new TypeBasic("bool");
            return new TypeBasic("cent");
        }
        else if(LLVMGetTypeKind(t) == LLVMFloatTypeKind) {
            if(t == LLVMFloatTypeInContext(Generator.Context)) return new TypeBasic("float");
            return new TypeBasic("double");
        }
        else if(LLVMGetTypeKind(t) == LLVMDoubleTypeKind) {
            return new TypeBasic("double");
        }
        else if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
            if(LLVMGetTypeKind(LLVMGetElementType(t)) == LLVMStructTypeKind) return new TypeStruct(cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(t))));
            return new TypePointer(llvmTypeToType(LLVMGetElementType(t)));
        }
        else if(LLVMGetTypeKind(t) == LLVMArrayTypeKind) {
            return new TypeArray(cast(int)LLVMGetArrayLength(t),llvmTypeToType(LLVMGetElementType(t)));
        }
        else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) return new TypeStruct(cast(string)fromStringz(LLVMGetStructName(t)));
        else if(LLVMGetTypeKind(t) == LLVMFunctionTypeKind) {
            return getType(Generator.typeToString(t)[0..Generator.typeToString(t).lastIndexOf('(')].strip());
        }
        assert(0);
}

string[] warningStack;
Type[string] aliasTypes;

class LLVMGen {
    LLVMModuleRef Module;
    LLVMContextRef Context;
    LLVMBuilderRef Builder;
    LLVMValueRef[string] Globals;
    LLVMValueRef[string] Functions;
    LLVMTypeRef[string] Structs;
    Loop[int] ActiveLoops;
    LLVMBasicBlockRef currBB;
    LLVMValueRef[string] LLVMFunctions;
    LLVMTypeRef[string] LLVMFunctionTypes;
    string file;
    long countOfLambdas;
    Type[string] toReplace;
    int currentBuiltinArg = 0;
    string lastFunc = "";
    CompOpts opts;
    LLVMTypeRef[BasicType] changeableTypes;

    this(string file, CompOpts opts) {
        this.file = file;
        this.opts = opts;
        Context = LLVMContextCreate();
        Module = LLVMModuleCreateWithNameInContext(toStringz("rave"),Context);
        LLVMInitializeAllTargets();
        LLVMInitializeAllAsmParsers();
        LLVMInitializeAllAsmPrinters();
        LLVMInitializeAllTargetInfos();
        LLVMInitializeAllTargetMCs();

        changeableTypes[BasicType.Bool] = LLVMInt1TypeInContext(Context);
        changeableTypes[BasicType.Char] = LLVMInt8TypeInContext(Context);
        changeableTypes[BasicType.Uchar] = LLVMInt8TypeInContext(Context);
        changeableTypes[BasicType.Short] = LLVMInt16TypeInContext(Context);
        changeableTypes[BasicType.Ushort] = LLVMInt16TypeInContext(Context);
        changeableTypes[BasicType.Int] = LLVMInt32TypeInContext(Context);
        changeableTypes[BasicType.Uint] = LLVMInt32TypeInContext(Context);
        changeableTypes[BasicType.Long] = LLVMInt64TypeInContext(Context);
        changeableTypes[BasicType.Ulong] = LLVMInt64TypeInContext(Context);
        changeableTypes[BasicType.Cent] = LLVMInt128TypeInContext(Context);
        changeableTypes[BasicType.Ucent] = LLVMInt128TypeInContext(Context);
        changeableTypes[BasicType.Float] = LLVMFloatTypeInContext(Context);
        changeableTypes[BasicType.Double] = LLVMDoubleTypeInContext(Context);

        NeededFunctions["free"] = "std::free";
        NeededFunctions["malloc"] = "std::malloc";
        if(opts.runtimeChecks) {
            NeededFunctions["assert"] = "std::assert[_b_p]";
        }
    }

    string mangle(string name, bool isFunc, bool isMethod, FuncArgSet[] args = null) {
        pragma(inline,true);
        if(isFunc) {
            string toAdd = "";
            //if(args !is null) toAdd = typesToString(args);
            if(isMethod) return "_RaveM"~to!string(name.length)~name~toAdd;
            return "_RaveF"~to!string(name.length)~name~toAdd;
        }
        return "_Raveg"~name;
    }

    void error(int loc, string msg) {
        pragma(inline,true);
        writeln("\033[0;31mError in '",file,"' file on "~to!string(loc)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
    }

    void warning(int loc, string msg) {
        pragma(inline,true);
        writeln("\033[33mWarning in '",file,"' file on "~to!string(loc)~" line: "~ msg ~"\033[0;0m");
    }

    Type getTrueStructType(TypeStruct ts) {
        if(ts.name.into(Generator.toReplace)) {
            Type _t = Generator.toReplace[ts.name];
            while(_t.toString().into(Generator.toReplace)) _t = Generator.toReplace[_t.toString()];
            return _t;
        }
        return ts;
    }

    Type[] getTrueTypeList(Type _t) {
        Type[] _list;

        while(_t.instanceof!TypePointer || _t.instanceof!TypeArray) {
            _list ~= _t;

            if(TypePointer _tp = _t.instanceof!TypePointer) {
                _t = _tp.instance;
            }
            else if(TypeArray _ta = _t.instanceof!TypeArray) {
                _t = _ta.element;
            }
        }

        _list ~= _t;
        return _list;
    }

    Type setByTypeList(Type[] _list) {
        Type[] list = _list.dup;

        if(list.length == 1) {
            if(list[0].instanceof!TypeStruct) return getTrueStructType(list[0].instanceof!TypeStruct);
            return list[0];
        }

        if(TypeStruct _ts = list[$-1].instanceof!TypeStruct) list[$-1] = getTrueStructType(_ts);

        for(int i=(cast(int)list.length)-1; i>0; i--) {
            if(TypePointer _tp = list[i-1].instanceof!TypePointer) {
                _tp.instance = list[i];
                list[i-1] = _tp;
            }
            else if(TypeArray _ta = list[i-1].instanceof!TypeArray) {
                _ta.element = list[i];
                list[i-1] = _ta;
            }
        }

        return list[0];
    }

    LLVMTypeRef GenerateType(Type t, int loc) {
        pragma(inline,true);
        if(t is null) return LLVMPointerType(
            LLVMInt8TypeInContext(Generator.Context),
            0
        );
        if(t.instanceof!TypeAlias) return null;
        if(t.instanceof!TypeBasic) {
            return changeableTypes[t.instanceof!TypeBasic.type];
        }
        if(TypePointer p = t.instanceof!TypePointer) {
            if(p.instance.instanceof!TypeVoid) {
                return LLVMPointerType(LLVMInt8TypeInContext(Generator.Context),0);
            }
            LLVMTypeRef a = LLVMPointerType(this.GenerateType(p.instance,loc),0);
            return a;
        }
        if(TypeArray a = t.instanceof!TypeArray) {
            return LLVMArrayType(this.GenerateType(a.element,loc),cast(uint)a.count);
        }
        if(TypeStruct s = t.instanceof!TypeStruct) {
            if(!s.name.into(Generator.Structs)) {
                if(s.name.into(Generator.toReplace)) {
                    return GenerateType(Generator.toReplace[s.name],loc);
                }
                if(s.name.indexOf('<') != -1) {
                    // Not-generated template
                    TypeStruct _s = s.copy().instanceof!TypeStruct;
                    for(int i=0; i<_s.types.length; i++) {
                        _s.types[i] = setByTypeList(getTrueTypeList(_s.types[i]));
                    }
                    _s.updateByTypes();

                    if(_s.name.into(Generator.Structs)) return Generator.Structs[_s.name];

                    string originalStructure = _s.name[0.._s.name.indexOf('<')];
                    if(originalStructure.into(StructTable)) {
                        return StructTable[originalStructure].generateWithTemplate(_s.name[_s.name.indexOf('<')..$],_s.types);
                    }
                }
                if(s.name.into(StructTable)) {
                    StructTable[s.name].check();
                    StructTable[s.name].generate();
                }
                else if(s.name.into(aliasTypes)) {
                    return Generator.GenerateType(aliasTypes[s.name],loc);
                }
                Generator.error(loc,"Unknown structure '"~s.name~"'!");
            }
            return Generator.Structs[s.name];
        }
        if(t.instanceof!TypeVoid) return LLVMVoidTypeInContext(Generator.Context);
        if(TypeFunc f = t.instanceof!TypeFunc) {
            if(f.main.instanceof!TypeVoid) f.main = new TypeBasic(BasicType.Bool);
            LLVMTypeRef[] types;
            foreach(ty; f.args) {
                types ~= Generator.GenerateType(ty,loc);
            }
            return LLVMPointerType(
                LLVMFunctionType(
                    Generator.GenerateType(f.main,loc),
                    types.ptr,
                    cast(uint)types.length,
                    false
                ),
                0
            );
        }
        if(TypeFuncArg fa = t.instanceof!TypeFuncArg) {
            return this.GenerateType(fa.type,loc);
        }
        if(TypeMacroArg ma = t.instanceof!TypeMacroArg) {
            if(NodeType nt = currScope.macroArgs[ma.num].instanceof!NodeType) return this.GenerateType(nt.ty,loc);
            if(NodeIden ni = currScope.macroArgs[ma.num].instanceof!NodeIden) {
                if(!ni.name.into(Generator.Structs)) {
                    if(ni.name.into(Generator.toReplace)) {
                        Type _t = Generator.toReplace[ni.name];
                        while(_t.toString().into(Generator.toReplace)) {
                            _t = Generator.toReplace[_t.toString()];
                        }
                        return this.GenerateType(_t,loc);
                    }
                    Generator.error(loc,"Unknown structure '"~ni.name~"'!");
                }
                return Generator.Structs[ni.name];
            }
        }
        if(TypeBuiltin tb = t.instanceof!TypeBuiltin) {
            NodeBuiltin nb = new NodeBuiltin(tb.name,tb.args.dup,loc,tb.block); nb.generate();
            return this.GenerateType(nb.ty,loc);
        }
        if(TypeConst tc = t.instanceof!TypeConst) return this.GenerateType(tc.instance,loc);
        assert(0);
    }

    LLVMTypeKind GenerateTypeKind(Type t) {
        pragma(inline,true);
        if(t.instanceof!TypeAlias) return -1;
        if(TypeBasic tb = t.instanceof!TypeBasic) {
            switch(tb.type) {
                case BasicType.Int:
                case BasicType.Long:
                case BasicType.Cent:
                case BasicType.Char:
                case BasicType.Bool:
                    return LLVMIntegerTypeKind;
                case BasicType.Float:
                    return LLVMFloatTypeKind;
                default: return LLVMDoubleTypeKind;
            }
        }
        if(TypePointer tp = t.instanceof!TypePointer) {
            if(tp.instance.instanceof!TypeStruct) return LLVMStructTypeKind;
            return LLVMPointerTypeKind;
        }
        if(TypeStruct ts = t.instanceof!TypeStruct) return LLVMStructTypeKind;
        if(TypeFunc tf = t.instanceof!TypeFunc) return LLVMFunctionTypeKind;
        return -1;
    }

    uint getAlignmentOfType(Type t) {
        pragma(inline,true);
        if(TypeBasic bt = t.instanceof!TypeBasic) {
            switch(bt.type) {
                case BasicType.Bool:
                case BasicType.Char:
                case BasicType.Uchar:
                    return 1;
                case BasicType.Short:
                case BasicType.Ushort:
                    return 2;
                case BasicType.Int:
                case BasicType.Float:
                case BasicType.Uint:
                    return 4;
                case BasicType.Long:
                case BasicType.Double:
                case BasicType.Ulong:
                    return 8;
                case BasicType.Cent:
                    return 16;
                default: assert(0);
            }
        }
        else if(TypePointer p = t.instanceof!TypePointer) {
            return 8;
        }
        else if(TypeArray arr = t.instanceof!TypeArray) {
            return getAlignmentOfType(arr.element);
        }
        else if(TypeStruct s = t.instanceof!TypeStruct) {
            return 8;
        }
        else if(TypeFunc f = t.instanceof!TypeFunc) {
            return getAlignmentOfType(f.main);
        }
        else if(TypeVoid v = t.instanceof!TypeVoid) {
            return 0;
        }
        else if(TypeAuto ta = t.instanceof!TypeAuto) return 0;
        else if(TypeConst tc = t.instanceof!TypeConst) return getAlignmentOfType(tc.instance);
        assert(0);
    }

    string typeToString(LLVMTypeRef t) {
        pragma(inline,true);
        return to!string(fromStringz(LLVMPrintTypeToString(t)));
    }

    LLVMValueRef byIndex(LLVMValueRef val, LLVMValueRef[] indexs) {
        pragma(inline,true);
        if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMArrayTypeKind) {
            return byIndex(LLVMBuildGEP(Generator.Builder,val,[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),0,false)].ptr,2,toStringz("gep222_")),indexs);
        }
        if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val))) == LLVMArrayTypeKind) {
            val = LLVMBuildPointerCast(Generator.Builder,val,LLVMPointerType(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(val))),0),toStringz("ptrcast"));
        }
        if(indexs.length > 1) {
            LLVMValueRef oneGep = LLVMBuildGEP(Generator.Builder,val,[indexs[0]].ptr,1,toStringz("gep225_"));
            return byIndex(oneGep,indexs[1..$]);
        }
        return LLVMBuildGEP(Generator.Builder,val,indexs.ptr,cast(uint)indexs.length,toStringz("gep225_"));
        assert(0);
    }

    void addAttribute(string name, LLVMAttributeIndex index, LLVMValueRef ptr, ulong value = 0) {
        int id = LLVMGetEnumAttributeKindForName(toStringz(name),cast(int)(name.length));
        if(id == 0) {
            error(-1,"Unknown attribute '"~name~"'!");
        }
        
        LLVMAttributeRef attr = LLVMCreateEnumAttribute(Generator.Context,id,value);
        LLVMAddAttributeAtIndex(ptr,index,attr);
    }

    void addStringAttribute(string name, LLVMAttributeIndex index, LLVMValueRef ptr, string val) {
        LLVMAttributeRef attr = LLVMCreateStringAttribute(Generator.Context,toStringz(name),cast(uint)name.length,toStringz(val),cast(uint)val.length);
        if(attr is null) {
            error(-1,"Unknown attribute '"~name~"'!");
        }
        LLVMAddAttributeAtIndex(ptr,index,attr);
    }
}

Node[int] condStack;

class Scope {
    LLVMValueRef[string] localscope;
    int[string] args;
    string func;
    LLVMBasicBlockRef BlockExit;
    bool funcHasRet = false;
    Node[int] macroArgs;
    NodeVar[string] localVars;
    NodeVar[string] argVars;
    bool isPure;
    bool inTry = false;

    this(string func, int[string] args, NodeVar[string] argVars) {
        this.func = func;
        this.args = args.dup;
        this.argVars = argVars.dup;
    }

    LLVMValueRef getWithoutLoad(string n, int loc = -1) {
        pragma(inline,true);
        if(!(n !in localscope)) return localscope[n];
        if(!(n !in Generator.Globals)) {
            if(isPure && !getVar(n,loc).isConst && !getVar(n,loc).t.instanceof!TypeAlias) {
                Generator.error(loc,"attempt to access a global variable inside a pure function!");
            }
            return Generator.Globals[n];
        }
        if(currScope.has("this") && !into(n,args)) {
            Type _t = currScope.getVar("this",loc).t;
            if(TypePointer tp = _t.instanceof!TypePointer) _t = tp.instance;
            TypeStruct ts = _t.instanceof!TypeStruct;

            if(ts.name.into(StructTable)) {
                if(cast(immutable)[ts.name,n].into(structsNumbers)) {
                    return new NodeGet(new NodeIden("this",loc),n,true,loc).generate();
                }
            }
        }
        if(!n.into(args)) {
            Generator.error(loc,"Undefined identifier '"~n~"'!");
        }
        return LLVMGetParam(
            Generator.Functions[func],
            cast(uint)args[n]
        );
    }

    void hasChanged(string n) {
        NodeVar v;
        if(n.into(localVars)) {
            v = localVars[n].instanceof!NodeVar;
            v.isChanged = true;
            localVars[n] = v;
        }
        if(n.into(Generator.Globals)) {
            VarTable[n].isChanged = true;
        }
        if(n.into(argVars)) {
            v = argVars[n].instanceof!NodeVar;
            v.isChanged = true;
            argVars[n] = v;
        }
    }

    LLVMValueRef get(string n, int loc = -1) {
        pragma(inline,true);
        if(!(n !in AliasTable)) return AliasTable[n].generate();
        if(!(n !in localscope)) {
            auto instr = LLVMBuildLoad(
                Generator.Builder,
                localscope[n],
                toStringz("loadLocal"~n~to!string(loc)~"_")
            );
            try {
                LLVMSetAlignment(instr,Generator.getAlignmentOfType(localVars[n].t));
            } catch(Error e) {}
            return instr;
        }
        if(!(n !in Generator.Globals)) {
            if(isPure && !getVar(n,loc).isConst && !getVar(n,loc).t.instanceof!TypeAlias) {
                Generator.error(loc,"attempt to access a global variable inside a pure function!");
            }
            auto instr = LLVMBuildLoad(
                Generator.Builder,
                Generator.Globals[n],
                toStringz("loadGlobal"~n~to!string(loc)~"_")
            );
            LLVMSetAlignment(instr,Generator.getAlignmentOfType(VarTable[n].t));
            return instr;
        }
        if(currScope.has("this") && !into(n,args) && !into(n,Generator.Functions)) {
            Type _t = currScope.getVar("this",loc).t;
            if(TypePointer tp = _t.instanceof!TypePointer) _t = tp.instance;
            TypeStruct ts = _t.instanceof!TypeStruct;

            if(ts.name.into(StructTable)) {
                if(cast(immutable)[ts.name,n].into(structsNumbers)) {
                    return new NodeGet(new NodeIden("this",loc),n,false,loc).generate();
                }
            }
        }
        if(func.into(Generator.Functions)) {
            if(!n.into(args)) {
                if(n.into(Generator.Functions)) return Generator.Functions[n];
                assert(0);
            }
            return LLVMGetParam(
                Generator.Functions[func],
                cast(uint)args[n]
            );
        }
        return LLVMGetParam(
            LambdaTable[func].f,
            cast(uint)args[n]
        );
    }

    NodeVar getVar(string n, int line) {
        pragma(inline,true);
        if(n.into(localscope)) return localVars[n];
        if(n.into(Generator.Globals)) return VarTable[n];
        if(n.into(args)) return argVars[n];
        if(currScope.has("this")) {
            Type _t = currScope.getVar("this",line).t;
            if(TypePointer tp = _t.instanceof!TypePointer) _t = tp.instance;
            TypeStruct ts = _t.instanceof!TypeStruct;

            if(ts.name.into(StructTable)) {
                if(cast(immutable)[ts.name,n].into(structsNumbers)) {
                    return structsNumbers[cast(immutable)[ts.name,n]].var;
                }
            }
        }
        Generator.error(line-1,"Undefined variable \""~n~"\"!");
        assert(0);
    }

    bool has(string n) {
        pragma(inline,true);
        return !(n !in AliasTable) || !(n !in localscope) || !(n !in Generator.Globals) || !(n !in args);
    }
}

struct StructMember {
    int number;
    NodeVar var;
}

LLVMGen Generator;
Scope currScope;
Scope prevScope;
int[NodeStruct] StructElements;

struct FuncArgSet {
    string name;
    Type type;
}

class Node {
    bool isChecked = false;

    this() {}

    LLVMValueRef generate() {
        writeln("Error: calling generate() method of Node!");
        exit(1);
        assert(0); // Compability
    }

    void debugPrint() {
        writeln("Node");
    }

    void check() {}

    Type getType() {
        writeln(this.classinfo);
        assert(0);
    }

    Node comptime() {return this;}

    Node copy() {return this;}
}

class NodeNone : Node {}

class NodeInt : Node {
    BigInt value;
    BasicType ty;
    Type _isVarVal = null;
    ubyte sys;
    bool isUnsigned = false;
    bool isMustBeLong = false;

    override Node copy() {
        return new NodeInt(value, sys);
    }

    this(BigInt value, int sys = 10) {
        this.value = value;
        this.sys = cast(ubyte)sys;
    }

    this(BigInt value, BasicType ty, Type _isVarVal, ubyte sys, bool isUnsigned, bool isMustBeLong) {
        this.value = value;
        this.ty = ty;
        if(_isVarVal !is null) this._isVarVal = _isVarVal.copy();
        else this._isVarVal = null;
        this.sys = sys;
        this.isUnsigned = isUnsigned;
        this.isMustBeLong = isMustBeLong;
    }

    this(int value, int sys = 10) {
        this.value = BigInt(value);
        this.sys = cast(ubyte)sys;
    }

    this(uint value, int sys = 10) {
        this.value = BigInt(value);
        this.sys = cast(ubyte)sys;
    }

    this(long value, int sys = 10) {
        this.value = BigInt(value);
        this.sys = cast(ubyte)sys;
    }

    this(ulong value, int sys = 10) {
        this.value = BigInt(value);
        this.sys = cast(ubyte)sys;
    }

    override Type getType() {
        if(isMustBeLong) return new TypeBasic("long");
        BasicType _type;
        if(_isVarVal.instanceof!TypeBasic) _type = ty = _isVarVal.instanceof!TypeBasic.type;
        else {
            if(value.toDecimalString().length < to!string(int.max).length) {
                _type = BasicType.Int;
            }
            else if(value.toDecimalString().length < to!string(long.max).length) {
                _type = BasicType.Long;
            }
            else _type = BasicType.Cent;
        }

        if(!isUnsigned) return new TypeBasic(_type);
        else {
            switch(_type) {
                case BasicType.Char: return new TypeBasic(BasicType.Uchar);
                case BasicType.Short: return new TypeBasic(BasicType.Ushort);
                case BasicType.Int: return new TypeBasic(BasicType.Uint);
                case BasicType.Long: return new TypeBasic(BasicType.Ulong);
                case BasicType.Cent: return new TypeBasic(BasicType.Ucent);
                default: return new TypeBasic(_type);
            }
        }
    }

    override LLVMValueRef generate() {
        if(isMustBeLong) return LLVMConstIntOfString(LLVMInt64TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
        if(_isVarVal.instanceof!TypeBasic) {
            ty = _isVarVal.instanceof!TypeBasic.type;
            switch(ty) {
                case BasicType.Bool: return LLVMConstIntOfString(LLVMInt1TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
                case BasicType.Uchar:
                case BasicType.Char: return LLVMConstIntOfString(LLVMInt8TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
                case BasicType.Ushort:
                case BasicType.Short: return LLVMConstIntOfString(LLVMInt16TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
                case BasicType.Uint:
                case BasicType.Int: return LLVMConstIntOfString(LLVMInt32TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
                case BasicType.Ulong:
                case BasicType.Long: return LLVMConstIntOfString(LLVMInt64TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
                case BasicType.Ucent:
                case BasicType.Cent: return LLVMConstIntOfString(LLVMInt128TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
                default: break;
            }
        }
        if(value.toDecimalString().length < to!string(int.max).length) {
            ty = BasicType.Int;
            return LLVMConstIntOfString(LLVMInt32TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
        }
        if(value.toDecimalString().length < to!string(long.max).length) {
            ty = BasicType.Long;
            return LLVMConstIntOfString(LLVMInt64TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
        }
        ty = BasicType.Cent;
        return LLVMConstIntOfString(LLVMInt128TypeInContext(Generator.Context),toStringz(value.toDecimalString()),this.sys);
        assert(0);
    }

    override void debugPrint() {
        writeln("NodeInt("~to!string(value)~")");
    }
}

class NodeFloat : Node {
    private double value;
    TypeBasic ty = null;

    override Node copy() {
        return new NodeFloat(value, ty);
    }

    this(double value) {
        this.value = value;
    }

    this(double value, bool v) {
        if(v) {
            ty = new TypeBasic(BasicType.Double);
        }
        this.value = value;
    }

    this(double value, TypeBasic ty) {
        this.value = value;
        this.ty = ty;
    }

    override Type getType() {
        if(ty !is null) return ty;
        if(value > float.max) ty = new TypeBasic(BasicType.Double);
        else ty = new TypeBasic(BasicType.Float);
        return ty;
    }

    override LLVMValueRef generate() {
        if(ty is null) {
            if(value > float.max) ty = new TypeBasic(BasicType.Double);
            else ty = new TypeBasic(BasicType.Float);
        }
        if(ty.type == BasicType.Double) return LLVMConstReal(LLVMDoubleTypeInContext(Generator.Context),value);
        return LLVMConstReal(LLVMFloatTypeInContext(Generator.Context),value);
    }

    override void debugPrint() {
        writeln("NodeFloat("~to!string(value)~")");
    }
}

class NodeString : Node {
    private string value;
    bool isWide = false;

    this(string value, bool isWide) {
        this.value = value;
        this.isWide = isWide;
    }

    override Node copy() {
        return new NodeString(value, isWide);
    }

    override Type getType() {
        return new TypePointer(new TypeBasic(BasicType.Char));
    }

    override LLVMValueRef generate() {
        LLVMValueRef globalstr;
        if(isWide) {
            globalstr = LLVMAddGlobal(
                Generator.Module,
                LLVMArrayType(LLVMInt32TypeInContext(Generator.Context),cast(uint)value.length+1),
                toStringz("_wstr")
            );
        }
        else globalstr = LLVMAddGlobal(
            Generator.Module,
            LLVMArrayType(LLVMInt8TypeInContext(Generator.Context),cast(uint)value.length+1),
            toStringz("_str")
        );
        LLVMSetGlobalConstant(globalstr, true);
        LLVMSetUnnamedAddr(globalstr, true);
        LLVMSetLinkage(globalstr, LLVMPrivateLinkage);
        if(!isWide) LLVMSetAlignment(globalstr, 1);
        else LLVMSetAlignment(globalstr, 4);
        LLVMSetInitializer(globalstr, LLVMConstStringInContext(Generator.Context, toStringz(value), cast(uint)value.length, false));
        return LLVMConstInBoundsGEP(
            globalstr,
            [
                LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),0,false),
                LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),0,false)
            ].ptr,
            2
        );
    }

    override void debugPrint() {
        writeln("NodeString(\""~to!string(value)~"\")");
    }
}

class NodeChar : Node {
    private char value;
    bool isWide = false;

    this(char value, bool isWide = false) {
        this.value = value;
        this.isWide = isWide;
    }

    override Node copy() {
        return new NodeChar(value, isWide);
    }

    override Type getType() {
        return new TypeBasic(BasicType.Char);
    }

    override LLVMValueRef generate() {
        if(!isWide) return LLVMConstInt(LLVMInt8TypeInContext(Generator.Context),to!ulong(value),false);
        return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),to!ulong(value),false);
    }

    override void debugPrint() {
        writeln("NodeChar('"~to!string(value)~"')");
    }
}

class NodeBinary : Node {
    TokType operator;
    Node first;
    Node second;
    bool isStatic = false;
    int loc;

    this(TokType operator, Node first, Node second, int line, bool isStatic = false) {
        this.first = first;
        this.second = second;
        this.operator = operator;
        this.loc = line;
        this.isStatic = isStatic;
    }

    override Node copy() {
        return new NodeBinary(operator, first.copy(), second.copy(), loc, isStatic);
    }

    LLVMValueRef mathOperation(LLVMValueRef one, LLVMValueRef two) {
        if(Generator.typeToString(LLVMTypeOf(one))[0..$-1] == Generator.typeToString(LLVMTypeOf(two)) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(one))) != LLVMStructTypeKind) {
            one = LLVMBuildLoad(Generator.Builder,one,toStringz("load1389_"));
        }
        else if(Generator.typeToString(LLVMTypeOf(two))[0..$-1] == Generator.typeToString(LLVMTypeOf(one)) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(two))) != LLVMStructTypeKind) {
            two = LLVMBuildLoad(Generator.Builder,two,toStringz("load1389_"));
        }

        if((LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMPointerTypeKind && fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(one)))).into(Generator.Structs))
         || (LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMStructTypeKind && fromStringz(LLVMGetStructName(LLVMTypeOf(one))).into(Generator.Structs))) {
            string structName = "";
            if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMPointerTypeKind) structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(one))));
            else if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMStructTypeKind) structName = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(one)));

            if(operator == TokType.Plus) {
                if(TokType.Plus.into(StructTable[structName].operators)) {
                    Type[] _types;

                    _types ~= llvmTypeToType(LLVMTypeOf(one));
                    _types ~= llvmTypeToType(LLVMTypeOf(two));
                    if(typesToString(_types).into(StructTable[structName].operators[TokType.Plus])) {
                        NodeFunc fn = StructTable[structName].operators[TokType.Plus][typesToString(_types)];
                        LLVMValueRef[] vals = [one,two];

                        return LLVMBuildCall(
                            Generator.Builder,
                            Generator.Functions[fn.name],
                            vals.ptr,
                            2,
                            toStringz(fn.type.instanceof!TypeVoid ? "" : "call")
                        );
                    }
                    else if("[]".into(StructTable[structName].operators[TokType.Plus])) {
                        NodeFunc fn = StructTable[structName].operators[TokType.Plus]["[]"];
                        LLVMValueRef[] vals = [one,two];

                        return LLVMBuildCall(
                            Generator.Builder,
                            Generator.Functions[fn.name],
                            vals.ptr,
                            2,
                            toStringz(fn.type.instanceof!TypeVoid ? "" : "call")
                        );
                    }
                }
            }
        }

        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two)) && LLVMTypeOf(one) != LLVMTypeOf(two)) {
            if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMIntegerTypeKind) {
                one = LLVMBuildIntCast(Generator.Builder,one,LLVMTypeOf(two),toStringz("intcast538_"));
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
                one = LLVMBuildFPCast(Generator.Builder,one,LLVMTypeOf(two),toStringz("fpcast541_"));
            }
        }

        if(LLVMGetTypeKind(LLVMTypeOf(one)) != LLVMGetTypeKind(LLVMTypeOf(two))) {
            if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMPointerTypeKind && LLVMGetElementType(LLVMTypeOf(one)) == LLVMTypeOf(two)) {
                one = LLVMBuildLoad(Generator.Builder,one,toStringz("load632_"));
            }
            Generator.error(loc,"value types are incompatible!");
        }
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMDoubleTypeKind) {
            switch(operator) {
                case TokType.Plus: return LLVMBuildFAdd(Generator.Builder, one, two, toStringz("fadd"));
                case TokType.Minus: return LLVMBuildFSub(Generator.Builder, one, two, toStringz("fsub"));
                case TokType.Multiply: return LLVMBuildFMul(Generator.Builder, one, two, toStringz("fmul"));
                case TokType.Divide: return LLVMBuildFDiv(Generator.Builder, one, two, toStringz("fdiv"));
                default: assert(0);
            }
        }
        switch(operator) {
            case TokType.Plus: return LLVMBuildAdd(Generator.Builder, one, two, toStringz("add"));
            case TokType.Minus: return LLVMBuildSub(Generator.Builder, one, two, toStringz("sub"));
            case TokType.Multiply: return LLVMBuildMul(Generator.Builder, one, two, toStringz("mul"));
            case TokType.Divide: return LLVMBuildSDiv(Generator.Builder, one, two, toStringz("div"));
            default: assert(0);
        }
    }

    string getStructName(LLVMTypeRef t) {
        pragma(inline,true);
        if(LLVMGetTypeKind(t) == LLVMStructTypeKind) return cast(string)fromStringz(LLVMGetStructName(t));
        return cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(t)));
    }

    LLVMValueRef equal(LLVMValueRef onev, LLVMValueRef twov, bool neq) {
        LLVMTypeRef one = LLVMTypeOf(onev);
        LLVMTypeRef two = LLVMTypeOf(twov);

        if(Generator.typeToString(one)[0..$-1] == Generator.typeToString(two)) {
            onev = LLVMBuildLoad(Generator.Builder,onev,toStringz("load1389_"));
        }
        else if(Generator.typeToString(two)[0..$-1] == Generator.typeToString(one)) {
            twov = LLVMBuildLoad(Generator.Builder,twov,toStringz("load1389_"));
        }

        string structName = "";
        bool isOperatorOverload = false;

        if((LLVMGetTypeKind(one) == LLVMPointerTypeKind || LLVMGetTypeKind(one) == LLVMStructTypeKind) && getStructName(one).into(StructTable)) {
            structName = getStructName(one);
            isOperatorOverload = true;
        }
        else if((LLVMGetTypeKind(two) == LLVMPointerTypeKind || LLVMGetTypeKind(two) == LLVMStructTypeKind) && getStructName(two).into(StructTable)) {
            structName = getStructName(two);
            isOperatorOverload = true;
        }

        if(isOperatorOverload) {
            if(operator == TokType.Equal) {
                if(TokType.Equal.into(StructTable[structName].operators)) {
                    Type[] _types; _types ~= llvmTypeToType(one); _types ~= llvmTypeToType(two);
                    if(typesToString(_types).into(StructTable[structName].operators[TokType.Equal]) || "[]".into(StructTable[structName].operators[TokType.Equal])) {
                        NodeFunc fn;
                        if("[]".into(StructTable[structName].operators[TokType.Equal])) {
                            fn = StructTable[structName].operators[TokType.Equal]["[]"];
                        }
                        else fn = StructTable[structName].operators[TokType.Equal][typesToString(_types)];
                        LLVMValueRef[] vals = [onev,twov];

                        return LLVMBuildCall(
                            Generator.Builder,
                            Generator.Functions[fn.name],
                            vals.ptr,
                            2,
                            toStringz(fn.type.instanceof!TypeVoid ? "" : "call")
                        );
                    }
                }
            }
            else if(operator == TokType.Nequal) {
                if(TokType.Nequal.into(StructTable[structName].operators)) {
                    Type[] _types; _types ~= llvmTypeToType(one); _types ~= llvmTypeToType(two);
                    //writeln(StructTable[structName].operators[TokType.Equal]);
                    if(typesToString(_types).into(StructTable[structName].operators[TokType.Nequal]) || "[]".into(StructTable[structName].operators[TokType.Nequal])) {
                        NodeFunc fn;
                        if("[]".into(StructTable[structName].operators[TokType.Nequal])) fn = StructTable[structName].operators[TokType.Nequal]["[]"];
                        else fn = StructTable[structName].operators[TokType.Nequal][typesToString(_types)];
                        LLVMValueRef[] vals = [onev,twov];

                        return LLVMBuildCall(
                            Generator.Builder,
                            Generator.Functions[fn.name],
                            vals.ptr,
                            2,
                            toStringz(fn.type.instanceof!TypeVoid ? "" : "call")
                        );
                    }
                }
            }
        }

        if(one == LLVMPointerType(two,0)) {
            onev = LLVMBuildLoad(Generator.Builder,onev,toStringz("load651_"));
        }
        else if(two == LLVMPointerType(one,0)) {
            twov = LLVMBuildLoad(Generator.Builder,twov,toStringz("load654_"));
        }

        if(LLVMGetTypeKind(one) == LLVMGetTypeKind(two) && one != two) {
            if(LLVMGetTypeKind(one) == LLVMIntegerTypeKind) {
                onev = LLVMBuildIntCast(Generator.Builder,onev,two,toStringz("intcast693_"));
            }
            else if(LLVMGetTypeKind(one) == LLVMFloatTypeKind || LLVMGetTypeKind(one) == LLVMDoubleTypeKind) {
                onev = LLVMBuildFPCast(Generator.Builder,onev,two,toStringz("fpcast696_"));
            }
        }

        if(LLVMGetTypeKind(one) == LLVMGetTypeKind(two)) {
            if(LLVMGetTypeKind(one) == LLVMIntegerTypeKind) {
                return LLVMBuildICmp(
                    Generator.Builder,
                    (neq ? LLVMIntNE : LLVMIntEQ),
                    onev,
                    twov,
                    toStringz("icmp")
                );
            }
            else if(LLVMGetTypeKind(one) == LLVMFloatTypeKind || LLVMGetTypeKind(one) == LLVMDoubleTypeKind) {
                return LLVMBuildFCmp(
                    Generator.Builder,
                    (neq ? LLVMRealONE : LLVMRealOEQ),
                    onev,
                    twov,
                    toStringz("fcmp")
                );
            }
            else if(LLVMGetTypeKind(one) == LLVMPointerTypeKind) {
                return LLVMBuildICmp(
                    Generator.Builder,
                    (neq ? LLVMIntNE : LLVMIntEQ),
                    LLVMBuildPtrToInt(
                        Generator.Builder,
                        onev,
                        LLVMInt32TypeInContext(Generator.Context),
                        toStringz("ptoi")
                    ),
                    LLVMBuildPtrToInt(
                        Generator.Builder,
                        twov,
                        LLVMInt32TypeInContext(Generator.Context),
                        toStringz("ptoi2")
                    ),
                    toStringz("pcmp")
                );
            }
        }
        writeln(loc," ",Generator.typeToString(one)," ",Generator.typeToString(two));
        assert(0);
    }

    LLVMValueRef diff(LLVMValueRef one, LLVMValueRef two, bool less, bool orequal) {
        if(Generator.typeToString(LLVMTypeOf(one))[0..$-1] == Generator.typeToString(LLVMTypeOf(two))) {
            one = LLVMBuildLoad(Generator.Builder,one,toStringz("load1389_"));
        }
        else if(Generator.typeToString(LLVMTypeOf(two))[0..$-1] == Generator.typeToString(LLVMTypeOf(one))) {
            two = LLVMBuildLoad(Generator.Builder,two,toStringz("load1389_"));
        }
        
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
            if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMIntegerTypeKind) {
                if(!orequal) return LLVMBuildICmp(
                    Generator.Builder,
                    (less ? LLVMIntSLT : LLVMIntSGT),
                    one,
                    two,
                    toStringz("icmp")
                );
                return LLVMBuildICmp(
                    Generator.Builder,
                    (less ? LLVMIntSLE : LLVMIntSGE),
                    one,
                    two,
                    toStringz("icmp")
                );
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMDoubleTypeKind) {
                if(!orequal) return LLVMBuildFCmp(
                    Generator.Builder,
                    (less ? LLVMRealOLT : LLVMRealOGT),
                    one,
                    two,
                    toStringz("fcmp")
                );
                return LLVMBuildFCmp(
                    Generator.Builder,
                    (less ? LLVMRealOLE : LLVMRealOGE),
                    one,
                    two,
                    toStringz("fcmp")
                );
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMPointerTypeKind) {
                if(!orequal) return LLVMBuildICmp(
                    Generator.Builder,
                    (less ? LLVMIntSLT : LLVMIntSGT),
                    LLVMBuildPtrToInt(
                        Generator.Builder,
                        one,
                        LLVMInt32TypeInContext(Generator.Context),
                        toStringz("ptoi")
                    ),
                    LLVMBuildPtrToInt(
                        Generator.Builder,
                        two,
                        LLVMInt32TypeInContext(Generator.Context),
                        toStringz("ptoi2")
                    ),
                    toStringz("pcmp")
                );
                return LLVMBuildICmp(
                    Generator.Builder,
                    (less ? LLVMIntSLE : LLVMIntSGE),
                    LLVMBuildPtrToInt(
                        Generator.Builder,
                        one,
                        LLVMInt32TypeInContext(Generator.Context),
                        toStringz("ptoi")
                    ),
                    LLVMBuildPtrToInt(
                        Generator.Builder,
                        two,
                        LLVMInt32TypeInContext(Generator.Context),
                        toStringz("ptoi2")
                    ),
                    toStringz("pcmp")
                );
            }
        }
        assert(0);
    }

    override Node comptime() {
        if(NodeBinary b = second.instanceof!NodeBinary) {
            b.isStatic = true;
        }

        if(operator == TokType.Equ) {
            AliasTable[first.instanceof!NodeIden.name] = second.comptime();
            return AliasTable[first.instanceof!NodeIden.name];
        }

        if(operator == TokType.PluEqu || operator == TokType.MinEqu || operator == TokType.MulEqu || operator == TokType.DivEqu) {
            TokType baseOperator;
            switch(operator) {
                case TokType.PluEqu: baseOperator = TokType.Plus; break;
                case TokType.MinEqu: baseOperator = TokType.Minus; break;
                case TokType.MulEqu: baseOperator = TokType.Multiply; break;
                default: baseOperator = TokType.Divide; break;
            }
            return new NodeBinary(TokType.Equ,first,new NodeBinary(baseOperator,first,second,loc),loc).comptime();
        }

        Node _first = first;
        while(_first.instanceof!NodeIden) {
            NodeIden id = _first.instanceof!NodeIden;
            _first = id.comptime();
        }
        Node _second = second;
        while(_second.instanceof!NodeIden) {
            NodeIden id = _second.instanceof!NodeIden;
            _second = id.comptime();
        }
        _first = _first.comptime();
        _second = _second.comptime();

        NodeInt[] asInts() {
            return [_first.instanceof!NodeInt,_second.instanceof!NodeInt];
        }
        NodeFloat[] asFloats() {
            return [_first.instanceof!NodeFloat,_second.instanceof!NodeFloat];
        }
        Type[] asTypes() {
            Type[] _types;
            if(_first.instanceof!NodeBuiltin) {
                _first.comptime();
                _types ~= _first.instanceof!NodeBuiltin.ty;
            }
            else _types ~= _first.comptime().instanceof!NodeType.ty;
            if(_second.instanceof!NodeBuiltin) {
                _second.comptime();
                _types ~= _second.instanceof!NodeBuiltin.ty;
            }
            else _types ~= _second.comptime().instanceof!NodeType.ty;
            return _types.dup;
        }
        NodeString[] asStrings() {
            return [_first.instanceof!NodeString,_second.instanceof!NodeString];
        }

        if(_first.instanceof!NodeInt && _second.instanceof!NodeFloat) {
            _first = new NodeFloat(cast(float)_first.instanceof!NodeInt.value);
        }
        else if(_second.instanceof!NodeInt && _first.instanceof!NodeFloat) {
            _second = new NodeFloat(cast(float)_second.instanceof!NodeInt.value);
        }

        if(_first.instanceof!NodeInt) {
            NodeInt[] ints = asInts();
            switch(operator) {
                case TokType.Plus: return new NodeInt(ints[0].value + ints[1].value);
                case TokType.Minus: return new NodeInt(ints[0].value - ints[1].value);
                case TokType.Multiply: return new NodeInt(ints[0].value * ints[1].value);
                case TokType.Divide: return new NodeInt(ints[0].value / ints[1].value);
                case TokType.Equal: return new NodeBool(ints[0].value == ints[1].value);
                case TokType.Nequal: return new NodeBool(ints[0].value != ints[1].value);
                case TokType.More: return new NodeBool(ints[0].value > ints[1].value);
                case TokType.Less: return new NodeBool(ints[0].value < ints[1].value);
                case TokType.LessEqual: return new NodeBool(ints[0].value <= ints[1].value);
                case TokType.MoreEqual: return new NodeBool(ints[0].value >= ints[1].value);
                default: break;
            }
        }
        else if(_first.instanceof!NodeFloat) {
            NodeFloat[] floats = asFloats();
            switch(operator) {
                case TokType.Plus: return new NodeFloat(floats[0].value + floats[1].value);
                case TokType.Minus: return new NodeFloat(floats[0].value - floats[1].value);
                case TokType.Multiply: return new NodeFloat(floats[0].value * floats[1].value);
                case TokType.Divide: return new NodeFloat(floats[0].value / floats[1].value);
                case TokType.Equal: return new NodeBool(floats[0].value == floats[1].value);
                case TokType.Nequal: return new NodeBool(floats[0].value != floats[1].value);
                case TokType.More: return new NodeBool(floats[0].value > floats[1].value);
                case TokType.Less: return new NodeBool(floats[0].value < floats[1].value);
                case TokType.LessEqual: return new NodeBool(floats[0].value <= floats[1].value);
                case TokType.MoreEqual: return new NodeBool(floats[0].value >= floats[1].value);
                default: break;
            }
        }
        else if(_first.instanceof!NodeType) {
            Type[] types = asTypes();
            switch(operator) {
                case TokType.Equal: return new NodeBool(types[0].toString() == types[1].toString());
                case TokType.Nequal: return new NodeBool(types[0].toString() != types[1].toString());
                default: break;
            }
        }
        else if(_first.instanceof!NodeString) {
            NodeString[] strings = asStrings();
            switch(operator) {
                case TokType.Equal: return new NodeBool(strings[0].value == strings[1].value);
                case TokType.Nequal: return new NodeBool(strings[0].value != strings[1].value);
                default: break;
            }
        }
        else if(_first.instanceof!NodeBool) {
            NodeBool[] bools = [_first.instanceof!NodeBool,_second.instanceof!NodeBool];
            switch(operator) {
                case TokType.And: return new NodeBool(bools[0].value && bools[1].value);
                case TokType.Or: return new NodeBool(bools[0].value || bools[1].value);
                case TokType.Equal: return new NodeBool(bools[0].value == bools[1].value);
                case TokType.Nequal: return new NodeBool(bools[0].value != bools[1].value);
                default: break;
            }
        }
        assert(0);
    }

    override Type getType() {
        switch(operator) {
            case TokType.Equ: case TokType.PluEqu:
            case TokType.MinEqu: case TokType.DivEqu:
            case TokType.MulEqu:
                return new TypeVoid();
            case TokType.Equal: case TokType.Nequal:
            case TokType.More: case TokType.Less:
            case TokType.MoreEqual: case TokType.LessEqual:
            case TokType.And: case TokType.Or:
                return new TypeBasic("bool");
            default:
                Type firstt = first.getType();
                Type secondt = second.getType();
                if(firstt.getSize() >= secondt.getSize()) return firstt;
                return secondt;
        }
    }

    override LLVMValueRef generate() {
        import std.algorithm : count;

        if(operator == TokType.PluEqu || operator == TokType.MinEqu || operator == TokType.MulEqu || operator == TokType.DivEqu) {
            TokType baseOperator;
            switch(operator) {
                case TokType.PluEqu: baseOperator = TokType.Plus; break;
                case TokType.MinEqu: baseOperator = TokType.Minus; break;
                case TokType.MulEqu: baseOperator = TokType.Multiply; break;
                default: baseOperator = TokType.Divide; break;
            }
            NodeBinary b = new NodeBinary(TokType.Equ,first,new NodeBinary(baseOperator,first,second,loc),loc);
            b.check();
            return b.generate();
        }

        bool isAliasIden = false;
        if(NodeIden i = first.instanceof!NodeIden) {isAliasIden = i.name.into(AliasTable);}

        if(operator == TokType.Equ) {
            if(NodeIden i = first.instanceof!NodeIden) {
                if(currScope.getVar(i.name,loc).isConst && i.name != "this" && currScope.getVar(i.name,loc).isChanged) {
                    Generator.error(loc, "An attempt to change the value of a constant variable!");
                }
                if(!currScope.getVar(i.name,loc).isChanged) currScope.hasChanged(i.name);
                if(isAliasIden) {
                    AliasTable[i.name] = second;
                    return null;
                }

                LLVMTypeRef ty = LLVMTypeOf(currScope.get(i.name,loc));

                if(NodeNull nn = second.instanceof!NodeNull) {
                    nn.maintype = null;
                    nn.llvmtype = ty;
                }

                LLVMValueRef val = second.generate();
                if(LLVMGetTypeKind(ty) == LLVMStructTypeKind
                || (LLVMGetTypeKind(ty) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(ty)) == LLVMStructTypeKind)) {
                    string structName;
                    if(LLVMGetTypeKind(ty) == LLVMPointerTypeKind) structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(ty)));
                    else structName = cast(string)fromStringz(LLVMGetStructName(ty));
                    if(TokType.Equ.into(StructTable[structName].operators)) {
                        Type[] types; types ~= llvmTypeToType(LLVMTypeOf(currScope.getWithoutLoad(i.name,loc))); types ~= llvmTypeToType(LLVMTypeOf(val));
                        if(typesToString(types).into(StructTable[structName].operators[TokType.Equ])) {
                            return LLVMBuildCall(
                                Generator.Builder,
                                Generator.Functions[StructTable[structName].operators[TokType.Equ][typesToString(types)].name],
                                [currScope.getWithoutLoad(i.name,loc),val].ptr,
                                2,
                                toStringz(StructTable[structName].operators[TokType.Equ][typesToString(types)].type.instanceof!TypeVoid ? "" : "call")
                            );
                        }
                    }
                }

                if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMGetTypeKind(ty) && LLVMGetTypeKind(ty) == LLVMIntegerTypeKind && LLVMTypeOf(val) != ty) {
                    val = LLVMBuildIntCast(Generator.Builder,val,ty,toStringz("intc_"));
                }
                else if(LLVMGetTypeKind(ty) == LLVMDoubleTypeKind && LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMFloatTypeKind) {
                    val = LLVMBuildFPCast(Generator.Builder,val,ty,toStringz("floatc_"));
                }
                else if(LLVMGetTypeKind(ty) == LLVMFloatTypeKind && LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMDoubleTypeKind) {
                    val = LLVMBuildFPCast(Generator.Builder,val,ty,toStringz("floatc_"));
                }
                else if(ty != LLVMTypeOf(val)) {
                    if(LLVMGetTypeKind(ty) == LLVMPointerTypeKind && LLVMTypeOf(val) == LLVMPointerType(LLVMInt8TypeInContext(Generator.Context),0)) {
                        val = LLVMBuildPointerCast(Generator.Builder, val, ty, toStringz("ptrc_"));
                    }
                    else Generator.error(loc,"The variable '"~i.name~"' with type '"~Generator.typeToString(ty)~"' is incompatible with value type '"~Generator.typeToString(LLVMTypeOf(val))~"'!");
                }

                return LLVMBuildStore(
                    Generator.Builder,
                    val,
                    currScope.getWithoutLoad(i.name,loc)
                );
            }
            else if(NodeGet g = first.instanceof!NodeGet) {
                LLVMValueRef _g = g.generate();
                if(NodeNull nn = second.instanceof!NodeNull) {
                    nn.maintype = null;
                    nn.llvmtype = LLVMTypeOf(g.generate());
                }
                LLVMValueRef val = second.generate();

                if(Generator.typeToString(LLVMTypeOf(val))[0..$-1] == Generator.typeToString(LLVMTypeOf(_g))) {
                    val = LLVMBuildLoad(Generator.Builder,val,toStringz("load764_1_"));
                }

                g.isMustBePtr = true;

                if(NodeIden id = g.base.instanceof!NodeIden) {
                    if(id.name.into(VarTable) && currScope.isPure) {
                        Generator.error(loc,"An attempt to change the global variable '"~id.name~"' in a pure function '"~currScope.func~"'!");
                    }
                }

                _g = g.generate();

                if(LLVMTypeOf(val) == LLVMTypeOf(_g)) {
                    if(second.instanceof!NodeNull) val = LLVMBuildLoad(Generator.Builder,val,"load_");
                }

                if(g.elementIsConst) {
                    Generator.error(loc, "An attempt to change the constant element!");
                }

                if(LLVMGetElementType(LLVMTypeOf(_g)) != LLVMTypeOf(val) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(_g))) == LLVMGetTypeKind(LLVMTypeOf(val))) {
                    if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind) {
                        val = LLVMBuildIntCast(Generator.Builder,val,LLVMGetElementType(LLVMTypeOf(_g)),toStringz("intc_"));
                    }
                }

                return LLVMBuildStore(
                    Generator.Builder,
                    val,
                    _g
                );
            }
            else if(NodeIndex ind = first.instanceof!NodeIndex) {
                ind.isMustBePtr = true;

                LLVMValueRef ptr = ind.generate();
                if(ind.elementIsConst) Generator.error(loc, "An attempt to change the constant element!");

                if(NodeNull nn = second.instanceof!NodeNull) {
                    nn.maintype = null;
                    nn.llvmtype = LLVMGetElementType(LLVMTypeOf(ptr));
                }
                LLVMValueRef val = second.generate();

                if(LLVMTypeOf(ptr) == LLVMPointerType(LLVMPointerType(LLVMTypeOf(val),0),0)) ptr = LLVMBuildLoad(Generator.Builder,ptr,toStringz("load784_"));

                if(LLVMGetElementType(LLVMTypeOf(ptr)) != LLVMTypeOf(val) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMGetTypeKind(LLVMTypeOf(val))) {
                    if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind) {
                        val = LLVMBuildIntCast(Generator.Builder,val,LLVMGetElementType(LLVMTypeOf(ptr)),toStringz("intc_"));
                    }
                }

                return LLVMBuildStore(Generator.Builder,val,ptr);
            }
            else if(NodeDone nd = first.instanceof!NodeDone) {
                LLVMValueRef ptr = nd.generate();
                LLVMValueRef value = second.generate();
                return LLVMBuildStore(Generator.Builder,value,ptr);
            }
            else if(NodeSlice ns = first.instanceof!NodeSlice) {
                return ns.binSet(second);
            }
        }

        if(isStatic) {
            if(operator != TokType.Plus && operator != TokType.Minus
             &&operator != TokType.Multiply) return null;
        }

        if(first.getType() != second.getType()) {
            if(NodeNull nn = first.instanceof!NodeNull) {
                nn.maintype = second.getType();
            }
            else if(NodeNull nn = second.instanceof!NodeNull) {
                nn.maintype = first.getType();
            }
        }

        if(operator == TokType.Rem) {
            if(TypeBasic tb = first.getType().instanceof!TypeBasic) {
                if(tb.type == BasicType.Float || tb.type == BasicType.Double) {
                    return new NodeBuiltin("fmodf",[first,second],loc,null).generate();
                }
                return new NodeCast(new TypeBasic(tb.type), new NodeBuiltin("fmodf",[first,second],loc,null),loc).generate();
            }
            assert(0);
        }

        LLVMValueRef f = first.generate();
        LLVMValueRef s = second.generate();

        if(NodeNull nn = first.instanceof!NodeNull) {
            nn.maintype = null;
            nn.llvmtype = LLVMTypeOf(s);
            f = nn.generate();
        }
        else if(NodeNull nn = second.instanceof!NodeNull) {
            nn.maintype = null;
            nn.llvmtype = LLVMTypeOf(f);
            s = nn.generate();
        }

        if(Generator.typeToString(LLVMTypeOf(f))[0..$-1] == Generator.typeToString(LLVMTypeOf(s))) {
            f = LLVMBuildLoad(Generator.Builder,f,toStringz("load1389_"));
        }
        if(Generator.typeToString(LLVMTypeOf(s))[0..$-1] == Generator.typeToString(LLVMTypeOf(f))) {
            s = LLVMBuildLoad(Generator.Builder,s,toStringz("load1389_"));
        }

        if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMGetTypeKind(LLVMTypeOf(s)) && LLVMTypeOf(f) != LLVMTypeOf(s)) {
            if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMIntegerTypeKind) f = LLVMBuildIntCast(
                Generator.Builder,
                f,
                LLVMTypeOf(s),
                toStringz("IntCast")
            );
        }

        if(LLVMGetTypeKind(LLVMTypeOf(f)) != LLVMGetTypeKind(LLVMTypeOf(s))) {
            auto kone = LLVMGetTypeKind(LLVMTypeOf(f));
            auto ktwo = LLVMGetTypeKind(LLVMTypeOf(s));
            if((kone == LLVMFloatTypeKind || kone == LLVMDoubleTypeKind) && ktwo == LLVMIntegerTypeKind) s = LLVMBuildSIToFP(
                Generator.Builder,
                s,
                LLVMTypeOf(f),
                toStringz("BinItoF")
            );
            else if(kone == LLVMIntegerTypeKind && (ktwo == LLVMFloatTypeKind || ktwo == LLVMDoubleTypeKind)) {
                f = LLVMBuildSIToFP(
                    Generator.Builder,
                    f,
                    LLVMTypeOf(s),
                    toStringz("BinItoF")
                );
            }
            else if(kone == LLVMFloatTypeKind && ktwo == LLVMDoubleTypeKind) {
                f = LLVMBuildFPCast(Generator.Builder,f,LLVMTypeOf(s),toStringz("BinFtoD"));
            }
            else if(kone == LLVMDoubleTypeKind && ktwo == LLVMFloatTypeKind) {
                s = LLVMBuildFPCast(Generator.Builder,s,LLVMTypeOf(f),toStringz("BinFtoD"));
            }
            else {
                writeln(Generator.typeToString(LLVMTypeOf(f)));
                writeln(Generator.typeToString(LLVMTypeOf(s)));
                writeln(operator);
                Generator.error(loc,"Value types are incompatible!");
            }
        }
        else if(LLVMTypeOf(f) != LLVMTypeOf(s)) {
            if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMIntegerTypeKind) {
                int fISize = to!int(to!string(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(f))))[1..$]);
                int sISize = to!int(to!string(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(s))))[1..$]);
                if(fISize>sISize) {
                    s = LLVMBuildIntCast(
                        Generator.Builder,
                        s,
                        LLVMTypeOf(f),
                        toStringz("intcast")
                    );
                }
                else {
                    f = LLVMBuildIntCast(
                        Generator.Builder,
                        f,
                        LLVMTypeOf(s),
                        toStringz("intcast")
                    );
                }
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMFloatTypeKind) {
                int fISize = to!int(to!string(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(f))))[1..$]);
                int sISize = to!int(to!string(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(s))))[1..$]);
                if(fISize>sISize) {
                    s = LLVMBuildFPCast(
                        Generator.Builder,
                        s,
                        LLVMTypeOf(f),
                        toStringz("floatcast")
                    );
                }
                else {
                    f = LLVMBuildFPCast(
                        Generator.Builder,
                        f,
                        LLVMTypeOf(s),
                        toStringz("floatcast")
                    );
                }
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMPointerTypeKind) {
                if(to!string(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(f)))).count('*') > to!string(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(s)))).count('*')) {
                    s = LLVMBuildPointerCast(
                        Generator.Builder,
                        s,
                        LLVMTypeOf(f),
                        toStringz("ptrcast")
                    );
                }
                else {
                    f = LLVMBuildPointerCast(
                        Generator.Builder,
                        f,
                        LLVMTypeOf(s),
                        toStringz("ptrcast")
                    );
                }
            }
        }

        switch(operator) {
            case TokType.Plus: case TokType.Minus:
            case TokType.Multiply: case TokType.Divide: return mathOperation(f,s);
            case TokType.Equal: return equal(f,s,false);
            case TokType.Nequal: return equal(f,s,true);
            case TokType.Less: return diff(f,s,true,false);
            case TokType.More: return diff(f,s,false,false);
            case TokType.LessEqual: return diff(f,s,true,true);
            case TokType.MoreEqual: return diff(f,s,false,true);
            case TokType.And: case TokType.BitAnd: return LLVMBuildAnd(Generator.Builder,f,s,toStringz("and"));
            case TokType.Or: case TokType.BitOr: return LLVMBuildOr(Generator.Builder,f,s,toStringz("or"));
            case TokType.BitXor: return LLVMBuildXor(Generator.Builder,f,s,toStringz("bitxor"));
            case TokType.BitLeft: return LLVMBuildShl(Generator.Builder,f,s,toStringz("bitleft"));
            case TokType.BitRight: return LLVMBuildLShr(Generator.Builder,f,s,toStringz("bitright"));
            default: writeln(operator); assert(0);
        }
        assert(0);
    }

    override void debugPrint() {
        writeln("NodeBinary {");
        first.debugPrint();
        writeln(to!string(operator));
        second.debugPrint();
        writeln("}");
    }
}

class NodeBlock : Node {
    Node[] nodes;

    this(Node[] nodes) {
        this.nodes = nodes.dup;
    }

    override Node copy() {
        Node[] _nodes;
        for(int i=0; i<nodes.length; i++) if(nodes[i] !is null) _nodes ~= nodes[i].copy();
        return new NodeBlock(_nodes.dup);
    }

    override LLVMValueRef generate() {
        for(int i=0; i<nodes.length; i++) {
            if(nodes[i] !is null) nodes[i].generate();
        }
        return null;
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) foreach(n; nodes) {
            if(n !is null) n.check();
        }
    }

    void setFuncName(string f) {
        foreach(n; nodes) {
            if(NodeWhile nw = n.instanceof!NodeWhile) {
                nw.func = f;
            }
            else if(NodeFor nf = n.instanceof!NodeFor) {
                nf.f = f;
            }
        }
    }

    override void debugPrint() {
        writeln("NodeBlock {");
        for(int i=0; i<nodes.length; i++) {
            nodes[i].debugPrint();
        }
        writeln("}");
    }
}

void structSetPredefines(Type t, LLVMValueRef var) {
    if(TypeStruct s = t.instanceof!TypeStruct) {
            if(!s.name.into(StructTable)) return;
            if(StructTable[s.name].predefines.length>0) {
                for(int i=0; i<StructTable[s.name].predefines.length; i++) {
                    if(StructTable[s.name].predefines[i].value is null) continue;
                    if(StructTable[s.name].predefines[i].isStruct) {
                        NodeVar v = StructTable[s.name].predefines[i].value.instanceof!NodeVar;
                        structSetPredefines(
                            v.t,
                            LLVMBuildStructGEP(
                                Generator.Builder,
                                var,
                                cast(uint)i,
                                toStringz("getStructElement")
                            )
                        );
                    }
                    LLVMBuildStore(
                        Generator.Builder,
                        StructTable[s.name].predefines[i].value.generate(),
                        LLVMBuildStructGEP(
                            Generator.Builder,
                            var,
                            cast(uint)i,
                            toStringz("getStructElement2")
                        )
                    );
                }
            }
        }
}

class NodeVar : Node {
    string name;
    Node value;
    bool isExtern;
    DeclMod[] mods;
    int loc;
    Type t;
    string[] namespacesNames;
    bool isConst;
    bool isGlobal;
    const string origname;
    bool isVolatile;

    bool isChanged = false;
    bool noZeroInit = false;
    bool isPrivate = false;

    override Type getType() {
        return this.t;
    }

    this(string name, Node value, bool isExt, bool isConst, bool isGlobal, DeclMod[] mods, int loc, Type t, bool isVolatile = false) {
        this.name = name;
        this.origname = name;
        this.value = value;
        this.isExtern = isExt;
        this.isConst = isConst;
        this.mods = mods.dup;
        this.loc = loc;
        this.t = t;
        this.isGlobal = isGlobal;
        this.isVolatile = isVolatile;

        for(int i=0; i<mods.length; i++) {
            if(mods[i].name == "private") isPrivate = true;
        }
    }

    this(string name, Node value, bool isExt, bool isConst, bool isGlobal, DeclMod[] mods, int loc, Type t, bool isVolatile, bool isChanged, bool noZeroInit) {
        this.name = name;
        this.origname = name;
        this.value = value;
        this.isExtern = isExt;
        this.isConst = isConst;
        this.mods = mods.dup;
        this.loc = loc;
        this.t = t;
        this.isGlobal = isGlobal;
        this.isVolatile = isVolatile;
        this.isChanged = isChanged;
        this.noZeroInit = noZeroInit;
    }

    override Node copy() {
        Node _value = null;
        if(value !is null) _value = value.copy();
        return new NodeVar(origname, _value, isExtern, isConst, isGlobal, mods, loc, t, isVolatile);
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) {
            if(namespacesNames.length>0) {
                name = namespacesNamesToString(namespacesNames,name);
            }
            if(isGlobal) VarTable[name] = this;
            if(!t.instanceof!TypeBasic && !aliasTypes.empty) {
                Type _nt = t.check(null);
                if(_nt !is null) this.t = _nt;
            }
            if(TypeStruct ts = t.instanceof!TypeStruct) {
                if(ts.types.length > 0 && Generator !is null && Generator.toReplace.length > 0) {
                    for(int i=0; i<ts.types.length; i++) {
                        Type _t = ts.types[i];
                        while(_t.toString().into(Generator.toReplace)) {
                            _t = Generator.toReplace[_t.toString()];
                        }
                        ts.types[i] = _t;
                    }
                }
            }
        }
    }

    override LLVMValueRef generate() {
        if(t.instanceof!TypeAlias) {
            AliasTable[name] = value;
            return null;
        }
        if(TypeBuiltin tb = t.instanceof!TypeBuiltin) {
            NodeBuiltin nb = new NodeBuiltin(tb.name,tb.args.dup,loc,tb.block);
            nb.generate();
            this.t = nb.ty;
        }
        if(t.instanceof!TypeAuto && value is null) Generator.error(loc,"using 'auto' without an explicit value is prohibited!");
        if(currScope is null) {
            // Global variable
            // Only const-values
            bool noMangling = false;
            string cMangle = "";

            for(int i=0; i<mods.length; i++) {
                while(mods[i].name.into(AliasTable)) {
                    if(NodeArray arr = AliasTable[mods[i].name].instanceof!NodeArray) {
                        mods[i].name = arr.values[0].instanceof!NodeString.value;
                        mods[i].value = arr.values[1].instanceof!NodeString.value;
                    }
                    else mods[i].name = AliasTable[mods[i].name].instanceof!NodeString.value;
                }
                if(mods[i].name == "C") noMangling = true;
                else if(mods[i].name == "volatile") isVolatile = true;
                else if(mods[i].name == "linkname") {
                    cMangle = mods[i].value;
                }
            }
            if(!t.instanceof!TypeAuto) {
                if(NodeInt ni = value.instanceof!NodeInt) {
                    ni._isVarVal = t;
                    value = ni;
                }
                Generator.Globals[name] = LLVMAddGlobal(
                    Generator.Module,
                    Generator.GenerateType(t,loc),
                    toStringz(cMangle == "" ? ((noMangling) ? name : Generator.mangle(name,false,false)) : cMangle)
                );
            }
            else {
                LLVMValueRef val = value.generate();
                Generator.Globals[name] = LLVMAddGlobal(
                    Generator.Module,
                    LLVMTypeOf(val),
                    toStringz((noMangling) ? name : Generator.mangle(name,false,false))
                );
                t = llvmTypeToType(LLVMTypeOf(val));
                LLVMSetInitializer(Generator.Globals[name],val);
                LLVMSetAlignment(Generator.Globals[name],Generator.getAlignmentOfType(t));
                if(isVolatile) LLVMSetVolatile(Generator.Globals[name],true);
                return null;
            }

            if(isExtern) {
                LLVMSetLinkage(Generator.Globals[name], LLVMExternalLinkage);
            }
            if(isVolatile) LLVMSetVolatile(Generator.Globals[name],true);
            // else LLVMSetLinkage(Generator.Globals[name],LLVMCommonLinkage);
            LLVMSetAlignment(Generator.Globals[name],Generator.getAlignmentOfType(t));
            if(value !is null && !isExtern) {
                LLVMSetInitializer(Generator.Globals[name], value.generate());
            }
            else if(!isExtern) {
                LLVMSetInitializer(Generator.Globals[name], LLVMConstNull(Generator.GenerateType(t,loc)));
            }
            return null;
        }
        else {
            // Local variables
            for(int i=0; i<mods.length; i++) {
                if(mods[i].name == "volatile") isVolatile = true;
                else if(mods[i].name == "nozeroinit") noZeroInit = true;
            }
            currScope.localVars[name] = this;

            if(t.instanceof!TypeAuto) {
                LLVMValueRef val = value.generate();
                currScope.localscope[name] = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(val),toStringz(name));
                t = llvmTypeToType(LLVMTypeOf(val));
                LLVMSetAlignment(currScope.localscope[name],Generator.getAlignmentOfType(t));

                LLVMBuildStore(Generator.Builder,val,currScope.localscope[name]);

                return currScope.localscope[name];
            }

            if(NodeInt ni = value.instanceof!NodeInt) {
                ni._isVarVal = t;
                value = ni;
            }

            LLVMTypeRef gT = Generator.GenerateType(t,loc);

            currScope.localscope[name] = LLVMBuildAlloca(
                Generator.Builder,
                gT,
                toStringz(name)
            );
            if(isVolatile) {
                LLVMSetVolatile(currScope.localscope[name],true);
            }
            structSetPredefines(t, currScope.localscope[name]);

            LLVMSetAlignment(currScope.getWithoutLoad(name,loc),Generator.getAlignmentOfType(t));

            if(value !is null) {
                new NodeBinary(TokType.Equ,new NodeIden(name,loc),value,loc).generate();
            }
            else if (!noZeroInit) {
                if(LLVMGetTypeKind(gT) == LLVMArrayTypeKind) {
                    LLVMValueRef[] _values;
                    for(int i=0; i<LLVMGetArrayLength(gT); i++) {
                        _values ~= LLVMConstNull(LLVMGetElementType(gT));
                    }
                    LLVMBuildStore(Generator.Builder,LLVMConstArray(LLVMGetElementType(gT),_values.ptr,cast(uint)_values.length),currScope.localscope[name]);
                }
                else if(LLVMGetTypeKind(gT) != LLVMStructTypeKind) {
                    LLVMBuildStore(Generator.Builder,LLVMConstNull(gT), currScope.localscope[name]);
                }
            }
            return currScope.localscope[name];
        }
    }

    override void debugPrint() {
        writeln("NodeVar("~name~",isExtern="~to!string(isExtern)~")");
    }
}

class NodeIden : Node {
    string name;
    int loc;
    bool isMustBePtr = false;

    this(string name, int loc) {
        this.name = name;
        this.loc = loc;
    }

    this(string name, int loc, bool isMustBePtr) {
        this.name = name;
        this.loc = loc;
        this.isMustBePtr = isMustBePtr;
    }

    override Node copy() {
        return new NodeIden(name, loc);
    }

    override Type getType() {
        if(name in AliasTable) return AliasTable[name].getType();
        if(name.into(FuncTable)) return FuncTable[name].getType();
        if(name.into(MacroTable)) return MacroTable[name].getType();
        if(!currScope.has(name)) {
            if(name.into(Generator.toReplace)) {
                string newname = Generator.toReplace[name].toString();
                NodeIden newni = new NodeIden(newname,loc);
                newni.isMustBePtr = isMustBePtr;
                return newni.getType();
            }
            Generator.error(loc,"Unknown identifier \""~name~"\"!");
            return null;
        }
        return currScope.getVar(name,loc).getType();
    }

    override LLVMValueRef generate() {
        if(name in AliasTable) return AliasTable[name].generate();
        if(name.into(Generator.Functions)) {
            Generator.addAttribute("noinline",LLVMAttributeFunctionIndex,Generator.Functions[name]);
            return Generator.Functions[name];
        }
        if(!currScope.has(name)) {
            Generator.error(loc,"Unknown identifier \""~name~"\"!");
            return null;
        }
        if(isMustBePtr) return currScope.getWithoutLoad(name,loc);
        return currScope.get(name,loc);
    }

    override Node comptime() {
        if(name !in AliasTable) {
            Generator.error(loc,"Unknown alias \""~name~"\"!");
            return null;
        }
        return AliasTable[name];
    }

    override void debugPrint() {
        writeln("NodeIden("~name~")");
    }
}

struct RetGenStmt {
    LLVMBasicBlockRef where;
    LLVMValueRef value;
}

string namespacesNamesToString(string[] namespacesNames,string n) {
    string ret = n;
    for(int i=0; i<namespacesNames.length; i++) {
        ret = namespacesNames[i]~"::"~ret;
    }
    return ret;
}

string typesToString(FuncArgSet[] args) {
    string data = "[";
    for(int i=0; i<args.length; i++) {
        Type t = args[i].type;
        if(TypeBasic tb = t.instanceof!TypeBasic) {
            switch(tb.type) {
                case BasicType.Float: data ~= "_f"; break;
                case BasicType.Double: data ~= "_d"; break;
                case BasicType.Int: data ~= "_i"; break;
                case BasicType.Cent: data ~= "_t"; break;
                case BasicType.Char: data ~= "_c"; break;
                case BasicType.Long: data ~= "_l"; break;
                case BasicType.Short: data ~= "_h"; break;
                case BasicType.Ushort: data ~= "_uh"; break;
                case BasicType.Uchar: data ~= "_uc"; break;
                case BasicType.Uint: data ~= "_ui"; break;
                case BasicType.Ulong: data ~= "_ul"; break;
                case BasicType.Ucent: data ~= "_ut"; break;
                default: data ~= "_b"; break;
            }
        }
        else if(TypePointer tp = t.instanceof!TypePointer) {
            if(TypeStruct ts = tp.instance.instanceof!TypeStruct) {
                if(ts.name.indexOf('<') == -1) data ~= "_s-"~ts.name;
                else data ~= "_s-"~ts.name[0..ts.name.indexOf('<')];
            }
            else data ~= "_p";
        }
        else if(TypeArray ta = t.instanceof!TypeArray) {
            data ~= "_a";
        }
        else if(TypeStruct ts = t.instanceof!TypeStruct) {
            if(ts.name.indexOf('<') == -1) data ~= "_s-"~ts.name;
            else data ~= "_s-"~ts.name[0..ts.name.indexOf('<')];
        }
        else if(TypeFunc tf = t.instanceof!TypeFunc) {
            data ~= "_func";
        }
        else if(TypeConst tc = t.instanceof!TypeConst) data ~= typesToString([FuncArgSet("",tc.instance)])[1..$-1];
    }
    return data ~ "]";
}

string typesToString(Type[] args) {
    string data = "[";
    for(int i=0; i<args.length; i++) {
        Type t = args[i];
        if(TypeBasic tb = t.instanceof!TypeBasic) {
            switch(tb.type) {
                case BasicType.Float: data ~= "_f"; break;
                case BasicType.Double: data ~= "_d"; break;
                case BasicType.Int: data ~= "_i"; break;
                case BasicType.Cent: data ~= "_t"; break;
                case BasicType.Char: data ~= "_c"; break;
                case BasicType.Long: data ~= "_l"; break;
                case BasicType.Short: data ~= "_h"; break;
                case BasicType.Ushort: data ~= "_uh"; break;
                case BasicType.Uchar: data ~= "_uc"; break;
                case BasicType.Uint: data ~= "_ui"; break;
                case BasicType.Ulong: data ~= "_ul"; break;
                case BasicType.Ucent: data ~= "_ut"; break;
                default: data ~= "_b"; break;
            }
        }
        else if(TypePointer tp = t.instanceof!TypePointer) {
            if(TypeStruct ts = tp.instance.instanceof!TypeStruct) {
                data ~= "_s-"~ts.name;
            }
            else data ~= "_p";
        }
        else if(TypeArray ta = t.instanceof!TypeArray) {
            data ~= "_a";
        }
        else if(TypeStruct ts = t.instanceof!TypeStruct) {
            data ~= "_s-"~ts.name;
        }
        else if(TypeFunc tf = t.instanceof!TypeFunc) {
            data ~= "_func";
        }
        else if(TypeConst tc = t.instanceof!TypeConst) data ~= typesToString([tc.instance])[1..$-1];
    }
    return data ~ "]";
}

Type[] stringToTypes(string args) {
    Type[] data;
    args = args[1..$-1];

    string[] _types = args.split("_");

    for(int i=0; i<_types.length; i++) {
        switch(_types[i]) {
            case "i": data ~= new TypeBasic("int"); break;
            case "ui": data ~= new TypeBasic("uint"); break;
            case "b": data ~= new TypeBasic("bool"); break;
            case "l": data ~= new TypeBasic("long"); break;
            case "ul": data ~= new TypeBasic("ulong"); break;
            case "f": data ~= new TypeBasic("float"); break;
            case "d": data ~= new TypeBasic("double"); break;
            case "t": data ~= new TypeBasic("cent"); break;
            case "ut": data ~= new TypeBasic("ucent"); break;
            case "h": data ~= new TypeBasic("short"); break;
            case "uh": data ~= new TypeBasic("ushort"); break;
            case "c": data ~= new TypeBasic("char"); break;
            case "uc": data ~= new TypeBasic("uchar"); break;
            case "func": data ~= new TypeFunc(null,null); break;
            case "p": data ~= new TypePointer(null); break;
            default:
                if(_types[i][0] == 's') data ~= new TypeStruct(_types[i][2..$]);
                break;
        }
    }

    return data.dup;
}

class NodeFunc : Node {
    string name;
    FuncArgSet[] args;
    NodeBlock block;
    bool isExtern;
    DeclMod[] mods;
    int loc;
    Type type;
    NodeRet[] rets;
    RetGenStmt[] genRets;
    LLVMBasicBlockRef exitBlock;
    LLVMBuilderRef builder;
    string[] namespacesNames;
    const string origname;

    bool isMethod = false;
    bool isVararg = false;
    bool isInline = false;
    bool isTemplatePart = false;
    bool isPure = false;
    bool isTemplate = false;
    bool isCtargs = false;
    bool isCtargsPart = false;
    bool isComdat = false;
    bool isSafe = false;
    bool isFakeMethod = false;
    bool isNoNamespaces = false;
    bool isNoChecks = false;
    bool isPrivate = false;

    string[] templateNames;
    Type[] _templateTypes;

    LLVMTypeRef[] genParamTypes;

    bool noCompile = false;

    override Type getType() {
        return this.type;
    }

    this(string name, FuncArgSet[] args, NodeBlock block, bool isExtern, DeclMod[] mods, int loc, Type type, string[] templateNames) {
        this.name = name;
        this.origname = name;
        this.args = args.dup;
        this.block = new NodeBlock(block.nodes.dup);
        this.isExtern = isExtern;
        this.mods = mods.dup;
        this.loc = loc;
        this.type = type;
        this.templateNames = templateNames.dup;

        for(int i=0; i<mods.length; i++) {
            if(mods[i].name == "private") isPrivate = true;
        }
    }

    override Node copy() {
        FuncArgSet[] _args;
        for(int i=0; i<args.length; i++) {
            _args ~= FuncArgSet(args[i].name, args[i].type.copy());
        }
        return new NodeFunc(origname, _args.dup, block.copy().instanceof!NodeBlock, isExtern, mods.dup, loc, type.copy(), templateNames.dup);
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) {
            for(int i=0; i<mods.length; i++) {
                if(mods[i].name == "method") {
                    Type _struct = args[0].type;
                    name = "{"~_struct.toString()~"}"~name;
                }
                else if(mods[i].name == "noNamespaces") {
                    isNoNamespaces = true;
                }
            }
            if(namespacesNames.length>0 && !isNoNamespaces) {
                name = namespacesNamesToString(namespacesNames,name);
            }

            if(!type.instanceof!TypeBasic && !aliasTypes.empty) {
                Type _nt = type.check(null);
                if(_nt !is null) this.type = _nt;
            }

            for(int i=0; i<args.length; i++) {
                if(args[i].type !is null && Generator !is null && Generator.toReplace !is null) while(args[i].type.toString().into(Generator.toReplace)) args[i].type = Generator.toReplace[args[i].type.toString()];
            }

            if(TypeStruct ts = type.instanceof!TypeStruct) {
                for(int i=0; i<ts.types.length; i++) {
                    while(ts.types[i].toString().into(Generator.toReplace)) {
                        ts.types[i] = Generator.toReplace[ts.types[i].toString()];
                    }
                }
                if(ts.types.length > 0) ts.updateByTypes();
                if(ts !is null && Generator !is null && Generator.toReplace !is null) {
                    while(type.toString().into(Generator.toReplace)) {
                        type = Generator.toReplace[type.toString()];
                    }
                }
            }

            string toAdd = typesToString(args);
            if(!(name !in FuncTable)) {
                if(typesToString(FuncTable[name].args) == toAdd) {
                    if(name != "std::printf") checkError(loc,"a function with '"~name~"' name already exists on "~to!string(FuncTable[name].loc+1)~" line!");
                }
                else {
                    name ~= toAdd;
                }
            }
            FuncTable[name] = this;
            for(int i=0; i<block.nodes.length; i++) {
                if(NodeRet r = block.nodes[i].instanceof!NodeRet) {
                    r.parent = name;
                }
                block.nodes[i].check();
            }
        }
    }

    LLVMTypeRef* getParameters(int callconv) {
        LLVMTypeRef[] p;
        for(int i=0; i<args.length; i++) {
            if(args[i].type.instanceof!TypeVarArg) {
                isVararg = true;
                args = args[1..$];
                return getParameters(callconv);
            }
            else if(args[i].type.instanceof!TypeStruct) {
                //p ~= Generator.GenerateType(new TypePointer(args[i].type),loc);
                if(!args[i].type.instanceof!TypeStruct.name.into(Generator.toReplace)) {
                    string oldname = args[i].name;
                    args[i].name = "_RaveStructArgument"~oldname;
                    block.nodes = new NodeVar(oldname,new NodeIden(args[i].name,loc),false,false,false,[],loc,args[i].type) ~ block.nodes;
                    p ~= Generator.GenerateType(args[i].type,loc);
                }
                else p ~= Generator.GenerateType(args[i].type,loc);
            }
            else if(!args[i].type.instanceof!TypePointer && !args[i].type.instanceof!TypeConst && !args[i].type.instanceof!TypeFunc) {
                string oldname = args[i].name;
                args[i].name = "_RaveTempVariable"~oldname;
                block.nodes = new NodeVar(oldname,new NodeIden(args[i].name,loc),false,false,false,[],loc,args[i].type,false) ~ block.nodes;
                p ~= Generator.GenerateType(args[i].type,loc);
            }
            else p ~= Generator.GenerateType(args[i].type,loc);
        }
        this.genParamTypes = p.dup;
        return p.ptr;
    }

    override LLVMValueRef generate() {
        if(noCompile) return null;
        if(!templateNames.empty && !isTemplate) {
            isExtern = false;
            return null;
        }
        string linkName = Generator.mangle(name,true,false,args);
        if(isMethod) linkName = Generator.mangle(name,true,true,args);

        int callconv = 0; // 0 == C

        NodeBuiltin[string] _builtins;

        if(name == "main") {
            linkName = "main";
            if(type.instanceof!TypeVoid) {
                type = new TypeBasic(BasicType.Int);
                block.nodes ~= new NodeRet(new NodeInt(BigInt(0)),loc,name);
            }
        }
        for(int i=0; i<mods.length; i++) {
            while(mods[i].name.into(AliasTable)) {
                if(NodeArray arr = AliasTable[mods[i].name].instanceof!NodeArray) {
                    mods[i].name = arr.values[0].instanceof!NodeString.value;
                    mods[i].value = arr.values[1].instanceof!NodeString.value;
                }
                else mods[i].name = AliasTable[mods[i].name].instanceof!NodeString.value;
            }
            switch(mods[i].name) {
                case "C": case "c": linkName = name; break;
                case "vararg": isVararg = true; break;
                case "fastcc": callconv = 1; break;
                case "coldcc": callconv = 2; break;
                case "cdecl32": callconv = 3; break;
                case "cdecl64": callconv = 4; break;
                case "stdcall": callconv = 5; break;
                case "inline": isInline = true; break;
                case "linkname": linkName = mods[i].value; break;
                case "pure": isPure = true; break;
                case "ctargs": isCtargs = true; return null;
                case "comdat": isComdat = true; break;
                case "safe": isSafe = true; break;
                case "nochecks": isNoChecks = true; break;
                default: if(mods[i].name[0] == '@') _builtins[mods[i].name[1..$]] = mods[i]._genValue.instanceof!NodeBuiltin; break;
            }
        }

        auto oldReplace = Generator.toReplace.dup;
        if(isTemplate) {
            Generator.toReplace.clear();
            for(int i=0; i<templateNames.length; i++) {
                Generator.toReplace[templateNames[i]] = _templateTypes[i];
            }
        }

        foreach(k; byKey(_builtins)) {
            if(_builtins[k].generate() != LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false)) {
                Generator.error(loc,"The '"~k~"' builtin failed when generating the function '"~name~"'!");
            }
        }

        LLVMTypeRef* parametersG = getParameters(callconv);

        LLVMTypeRef functype = LLVMFunctionType(
            Generator.GenerateType(type,loc),
            parametersG,
            cast(uint)args.length,
            isVararg
        );

        Generator.Functions[name] = LLVMAddFunction(
            Generator.Module,
            toStringz(linkName),
            functype
        );

        if(callconv == 1) LLVMSetFunctionCallConv(Generator.Functions[name],LLVMFastCallConv);
        else if(callconv == 2) LLVMSetFunctionCallConv(Generator.Functions[name],LLVMColdCallConv);
        else if(callconv == 5) LLVMSetFunctionCallConv(Generator.Functions[name],LLVMX86StdcallCallConv);

        if(isInline) Generator.addAttribute("alwaysinline",LLVMAttributeFunctionIndex,Generator.Functions[name]);
        if(isTemplatePart || isTemplate || isCtargsPart || isComdat) {
            LLVMComdatRef cmr = LLVMGetOrInsertComdat(Generator.Module,toStringz(linkName));
            LLVMSetComdatSelectionKind(cmr,LLVMAnyComdatSelectionKind);
            LLVMSetComdat(Generator.Functions[name],cmr);
            LLVMSetLinkage(Generator.Functions[name],LLVMLinkOnceODRLinkage);
        }
        
        if(!isExtern) {
            if(isCtargsPart || isCtargs) Generator.currentBuiltinArg = 0;
            LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
                Generator.Context,
                Generator.Functions[name],
                toStringz("entry")
            );

            exitBlock = LLVMAppendBasicBlockInContext(
                Generator.Context,
                Generator.Functions[name],
                toStringz("exit")
            );

            builder = LLVMCreateBuilderInContext(Generator.Context);
            Generator.Builder = builder;

            LLVMPositionBuilderAtEnd(
                Generator.Builder,
                entry
            );

            int[string] intargs;
            NodeVar[string] argVars;

            for(int i=0; i<args.length; i++) {
                intargs[args[i].name] = i;
                argVars[args[i].name] = new NodeVar(args[i].name,null,false,true,false,[],loc,args[i].type);
            }

            currScope = new Scope(name,intargs,argVars.dup);
            currScope.isPure = isPure;
            Generator.currBB = entry;

            block.generate();

            if(type.instanceof!TypeVoid && !currScope.funcHasRet) LLVMBuildBr(
                Generator.Builder,
                exitBlock
            );

            LLVMMoveBasicBlockAfter(exitBlock, LLVMGetLastBasicBlock(Generator.Functions[name]));
            LLVMPositionBuilderAtEnd(Generator.Builder, exitBlock);

            if(!type.instanceof!TypeVoid) {
                RetGenStmt[] _newrets;
                for(int i=0; i<genRets.length; i++) {
                    if((i+1)<genRets.length && (fromStringz(LLVMGetBasicBlockName(genRets[i+1].where)) == fromStringz(LLVMGetBasicBlockName(genRets[i].where)))) {
                        if(fromStringz(LLVMGetValueName(genRets[i+1].value)) != fromStringz(LLVMGetValueName(genRets[i].value))) {
                            _newrets ~= genRets[i];
                        }
                    }
                    else _newrets ~= genRets[i];
                }
                genRets = _newrets.dup;

				LLVMValueRef[] retValues = array(map!(x => x.value)(genRets));
				LLVMBasicBlockRef[] retBlocks = array(map!(x => x.where)(genRets));

                if(retBlocks is null || retBlocks.length == 0) {
                    LLVMBuildRet(
                        Generator.Builder,
                        LLVMConstNull(Generator.GenerateType(type,loc))
                    );
                }
				else if(retBlocks.length == 1 || (retBlocks.length>0 && retBlocks[0] == null)) {
					LLVMBuildRet(Generator.Builder, retValues[0]);
				}
				else {
					auto phi = LLVMBuildPhi(Generator.Builder, Generator.GenerateType(type,loc), "retval");
					LLVMAddIncoming(
						phi,
						retValues.ptr,
						retBlocks.ptr,
						cast(uint)genRets.length
					);
					LLVMBuildRet(Generator.Builder, phi);
				}
			}
			else {
                LLVMBuildRetVoid(Generator.Builder);
            }

			Generator.Builder = null;
            currScope = null;
        }
        if(isTemplate) Generator.toReplace = oldReplace.dup;
        //writeln(fromStringz(LLVMPrintValueToString(Generator.Functions[name])));
        LLVMVerifyFunction(Generator.Functions[name],0);

        return Generator.Functions[name];
    }

    LLVMValueRef generateWithTemplate(Type[] _types, string _all) {
        auto activeLoops = Generator.ActiveLoops.dup;
        auto builder = Generator.Builder;
        auto currBB = Generator.currBB;
        auto _scope = currScope;

        NodeFunc _f = new NodeFunc(_all,args.dup,block,false,mods.dup,loc,type,templateNames.dup);
        _f.isTemplate = true;
        _f.check();
        _f._templateTypes = _types.dup;
        LLVMValueRef v = _f.generate();

        Generator.ActiveLoops = activeLoops.dup;
        Generator.Builder = builder;
        Generator.currBB = currBB;
        currScope = _scope;

        return v;
    }

    string generateWithCtargs(LLVMTypeRef[] args) {
        FuncArgSet[] newargs;
        for(int i=0; i<args.length; i++) {
            Type t = llvmTypeToType(args[i]);
            newargs ~= FuncArgSet("_RaveArg"~to!string(i),t);
        }

        auto activeLoops = Generator.ActiveLoops.dup;
        auto builder = Generator.Builder;
        auto currBB = Generator.currBB;
        auto _scope = currScope;

        DeclMod[] _mods;
        for(int i=0; i<mods.length; i++) {
            if(mods[i].name != "ctargs") _mods ~= mods[i];
        }

        NodeFunc _f = new NodeFunc(name~typesToString(newargs.dup),newargs.dup,block,false,_mods.dup,loc,type,[]);
        _f.args = newargs.dup;
        _f.isCtargsPart = true;
        string _n = _f.name;
        _f.check();
        _f.generate();

        Generator.ActiveLoops = activeLoops.dup;
        Generator.Builder = builder;
        Generator.currBB = currBB;
        currScope = _scope;

        return _n;
    }

    override void debugPrint() {
        writeln("NodeFunc("~name~",isExtern="~to!string(isExtern)~")");
    }
}

class NodeRet : Node {
    Node val;
    string parent;
    int loc;

    this(Node val, int loc, string parent) {
        this.val = val;
        this.loc = loc;
        this.parent = parent;
    }

    override Node copy() {
        return new NodeRet(val, loc, parent);
    }

    override Type getType() {
        return val.getType();
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(parent.into(FuncTable) && !oldCheck) FuncTable[parent].rets ~= this;
    }

    Loop getParentBlock(int n = -1) {
        if(Generator.ActiveLoops.length == 0) return Loop(false,null,null,false,false,null);
        return Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1];
    }

    void setParentBlock(Loop value, int n = -1) {
        if(Generator.ActiveLoops.length > 0) {
            if(n == -1) Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1] = value;
            else Generator.ActiveLoops[n] = value;
        }
    }

    override LLVMValueRef generate() {
        LLVMValueRef ret;
        if(parent.into(FuncTable)) {
            if(val !is null) {
                LLVMBasicBlockRef _start = Generator.currBB;
                if(Generator.ActiveLoops.length != 0) {
                    Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].hasEnd = true;
                    Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].rets ~= LoopReturn(this,cast(int)Generator.ActiveLoops.length-1);
                    //_start = Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].start;
                }
                if(NodeNull n = val.instanceof!NodeNull) {
                    n.maintype = FuncTable[parent].type;
                    LLVMValueRef retval = n.generate();
                    FuncTable[parent].genRets ~= RetGenStmt(_start,n.generate());
			        ret = LLVMBuildBr(Generator.Builder, FuncTable[parent].exitBlock);
                    return retval;
                }
                LLVMValueRef retval = val.generate();
                if(Generator.typeToString(LLVMTypeOf(retval))[0..$-1] == Generator.typeToString(Generator.GenerateType(FuncTable[parent].type,loc))) {
                    retval = LLVMBuildLoad(Generator.Builder,retval,toStringz("load1389_"));
                }
                FuncTable[parent].genRets ~= RetGenStmt(_start,retval);
			    ret = LLVMBuildBr(Generator.Builder, FuncTable[parent].exitBlock);
                return retval;
            }
            else ret = LLVMBuildBr(Generator.Builder,FuncTable[parent].exitBlock);
        }
        else if(parent == "lambda"~to!string(Generator.countOfLambdas-1)) {
            if(val !is null) {
                if(Generator.ActiveLoops.length != 0) {
                    Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].hasEnd = true;
                    Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].rets ~= LoopReturn(this,cast(int)Generator.ActiveLoops.length-1);
                }
                LLVMValueRef retval = val.generate();
                LambdaTable[parent].genRets ~= RetGenStmt(Generator.currBB,retval);
			    ret = LLVMBuildBr(Generator.Builder, LambdaTable[parent].exitBlock);
                return retval;
            }
            else ret = LLVMBuildBr(Generator.Builder,LambdaTable[parent].exitBlock);
        }
        else {
            return val.generate();
        }
        currScope.funcHasRet = true;
        assert(0);
    }
}

class NodeCall : Node {
    int loc;
    Node func;
    Node[] args;
    bool _inverted = false;

    this(int loc, Node func, Node[] args) {
        this.loc = loc;
        this.func = func;
        this.args = args.dup;
    }

    override Type getType() {
        if(NodeIden id = func.instanceof!NodeIden) {
            if((id.name ~ typesToString(getTypes())).into(FuncTable)) return FuncTable[(id.name ~ typesToString(getTypes()))].getType();
        }
        return func.getType();
    }

    Type[] getTypes() {
        Type[] arr;
        for(int i=0; i<args.length; i++) {
            arr ~= args[i].getType();
        }
        return arr.dup;
    }

    NodeFunc calledFunc = null;
    int _offset = 0;

    LLVMValueRef[] correctByLLVM(LLVMValueRef[] _tC) {
        pragma(inline,true);
        if(calledFunc is null || _tC.length == 0) return _tC;

        LLVMValueRef[] corrected = _tC.dup;
        LLVMTypeRef[] _types = calledFunc.genParamTypes;

        if(_types.length == 0) return _tC;

        for(int i=0; i<corrected.length; i++) {
            if(LLVMTypeOf(corrected[i]) != _types[i+_offset]) {
                if(LLVMTypeOf(corrected[i]) == LLVMPointerType(_types[i+_offset],0)) {
                    corrected[i] = LLVMBuildLoad(Generator.Builder,corrected[i],toStringz("load_"));
                }
                else if(_types[i] == LLVMPointerType(LLVMTypeOf(corrected[i]),0)) {
                    LLVMValueRef v = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(corrected[i]),toStringz("temp_"));
                    LLVMBuildStore(Generator.Builder,corrected[i],v);
                    corrected[i] = v;
                }
                else if(LLVMGetTypeKind(LLVMTypeOf(corrected[i])) == LLVMIntegerTypeKind && LLVMGetTypeKind(_types[i+_offset]) == LLVMIntegerTypeKind) {
                    corrected[i] = LLVMBuildIntCast(Generator.Builder,corrected[i],_types[i],toStringz("intc"));
                }
                else if(LLVMGetTypeKind(LLVMTypeOf(corrected[i])) == LLVMFloatTypeKind && LLVMGetTypeKind(_types[i+_offset]) == LLVMDoubleTypeKind) {
                    corrected[i] = LLVMBuildFPCast(Generator.Builder,corrected[i],_types[i],toStringz("floatc"));
                }
                else if(LLVMGetTypeKind(LLVMTypeOf(corrected[i])) == LLVMDoubleTypeKind && LLVMGetTypeKind(_types[i+_offset]) == LLVMFloatTypeKind) {
                    corrected[i] = LLVMBuildFPCast(Generator.Builder,corrected[i],_types[i],toStringz("floatc"));
                }
            }
        }

        return corrected.dup;
    }

    LLVMValueRef[] getParameters(bool isVararg, FuncArgSet[] fas = null) {
        LLVMValueRef[] params;

        if(isVararg) {
            for(int i=0; i<args.length; i++) params ~= args[i].generate();
            return params;
        }

        if(fas !is null && !fas.empty) {
            int offset = 0;
            if(fas[0].name == "this") {
                offset = 1;
                if(args.length >= 2) {
                    if(NodeIden t1 = args[0].instanceof!NodeIden) {
                        if(NodeIden t2 = args[1].instanceof!NodeIden) {
                            if(t1.name == t2.name) {
                                if(fas.length >= 2 && fas[0].name != fas[1].name) {
                                    args = args[1..$];
                                }
                            }
                        }
                    }
                }
            }

            for(int i = 0; i<fas.length; i++) {
                if(i == args.length) break;
                LLVMValueRef arg = args[i].generate();

                if(arg is null) continue;

                if(TypePointer tp = fas[i].type.instanceof!TypePointer) {
                    if(tp.instance.instanceof!TypeVoid) {
                        if(LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMPointerTypeKind) {
                            arg = new NodeCast(new TypePointer(new TypeVoid()),args[i],loc).generate();
                        }
                    }
                    if(tp.instance.instanceof!TypeStruct && LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMPointerTypeKind) {
                        if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(arg))) == LLVMPointerTypeKind) arg = LLVMBuildLoad(Generator.Builder,arg,toStringz("load1698_"));
                    }
                    else if(tp.instance.instanceof!TypeStruct && LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind) {
                        if(LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind) {
                            if(NodeIden id = args[i].instanceof!NodeIden) {
                                id.isMustBePtr = true;
                                arg = id.generate();
                            }
                            else if(NodeIndex ind = args[i].instanceof!NodeIndex) {
                                ind.isMustBePtr = true;
                                arg = ind.generate();
                            }
                            else if(NodeGet ng = args[i].instanceof!NodeGet) {
                                ng.isMustBePtr = true;
                                arg = ng.generate();
                            }
                        }
                    }

                    if(LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind) {
                        if(NodeIden id = args[i].instanceof!NodeIden) {id.isMustBePtr = true; arg = id.generate();}
                        else if(NodeGet ng = args[i].instanceof!NodeGet) {ng.isMustBePtr = true; arg = ng.generate();}
                        else if(NodeIndex ind = args[i].instanceof!NodeIndex) {ind.isMustBePtr = true; arg = ind.generate();}

                        if(LLVMGetTypeKind(LLVMTypeOf(arg)) != LLVMPointerTypeKind && (fas[i].name != "this" || LLVMGetTypeKind(LLVMTypeOf(arg)) == LLVMStructTypeKind)) {
                            // Create local copy
                            LLVMValueRef temp = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(arg),toStringz("tempArg_"));
                            LLVMBuildStore(Generator.Builder,arg,temp);
                            arg = temp;
                        }
                    }
                }
                params ~= arg;
            }
        }
        else for(int i=0; i<args.length; i++) {
            if(args[i].instanceof!NodeType) continue;
            LLVMValueRef arg = args[i].generate();
            params ~= arg;
        }

        return correctByLLVM(params);
    }

    Type[] parametersToTypes(LLVMValueRef[] params) {
        if(params is null) return [];
        Type[] types;

        for(int i=0; i<params.length; i++) {
            if(params[i] is null) continue;
            LLVMTypeRef t = LLVMTypeOf(params[i]);
            if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
                if(LLVMGetTypeKind(LLVMGetElementType(t)) == LLVMStructTypeKind) {
                    types ~= new TypeStruct(cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(t))));
                }
                else types ~= new TypePointer(null);
            }
            else if(LLVMGetTypeKind(t) == LLVMIntegerTypeKind) {
                if(t == LLVMInt32TypeInContext(Generator.Context)) types ~= new TypeBasic("int");
                else if(t == LLVMInt64TypeInContext(Generator.Context)) types ~= new TypeBasic("long");
                else if(t == LLVMInt16TypeInContext(Generator.Context)) types ~= new TypeBasic("short");
                else if(t == LLVMInt8TypeInContext(Generator.Context)) types ~= new TypeBasic("char");
                else if(t == LLVMInt1TypeInContext(Generator.Context)) types ~= new TypeBasic("bool");
                else types ~= new TypeBasic("cent");
            }
            else if(LLVMGetTypeKind(t) == LLVMFloatTypeKind) types ~= new TypeBasic("float");
            else if(LLVMGetTypeKind(t) == LLVMDoubleTypeKind) types ~= new TypeBasic("double");
            else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) types ~= new TypeStruct(cast(string)fromStringz(LLVMGetStructName(t)));
            else if(LLVMGetTypeKind(t) == LLVMArrayTypeKind) types ~= new TypeArray(0,null);
        }

        return types.dup;
    }

    override Node comptime() {
        Generator.error(loc,"Attempt to call a function during compilation!");
        return null;
    }

    override LLVMValueRef generate() {
        import lexer.lexer : Lexer;

        if(NodeIden f = func.instanceof!NodeIden) {
        if(!f.name.into(FuncTable) && !into(f.name~to!string(args.length),FuncTable) && !f.name.into(Generator.LLVMFunctions) && !into(f.name~typesToString(getTypes()),FuncTable)) {
            if(!f.name.into(MixinTable)) {
                if(currScope.has(f.name)) {
                    NodeVar nv = currScope.getVar(f.name,loc);
                    TypeFunc tf = nv.t.instanceof!TypeFunc;
                    string name = "CallVar"~f.name;
                    return LLVMBuildCall(
                        Generator.Builder,
                        currScope.get(f.name,loc),
                        getParameters(false).ptr,
                        cast(uint)args.length,
                        toStringz(tf.main.instanceof!TypeVoid ? "" : name)
                    );
                }
                if(f.name.into(Generator.toReplace)) {
                    f.name = Generator.toReplace[f.name].toString();
                    return generate();
                }
                if(f.name.indexOf('<') != -1) {
                    if(!f.name[0..f.name.indexOf('<')].into(FuncTable)) {
                        Lexer l = new Lexer(f.name,1);
                        Parser p = new Parser(l.getTokens(),1,"(builtin)");
                        TypeStruct _t = p.parseType().instanceof!TypeStruct;

                        bool _replaced = false;
                        for(int i=0; i<_t.types.length; i++) {
                            if(_t.types[i].toString().into(Generator.toReplace)) {
                                _t.types[i] = Generator.toReplace[_t.types[i].toString()];
                                _replaced = true;
                            }
                        }

                        if(_replaced) {
                            _t.updateByTypes();
                            NodeCall nc = new NodeCall(loc,new NodeIden(_t.name,f.loc),args.dup);
                            if(_t.name.into(FuncTable)) {
                                return nc.generate();
                            }
                            else if(_t.name[0.._t.name.indexOf('<')].into(FuncTable)) {
                                FuncTable[_t.name[0.._t.name.indexOf('<')]].generateWithTemplate(_t.types,_t.name);
                                return nc.generate();
                            }
                        }
                        else if(f.name[0..f.name.indexOf('<')].into(StructTable)) {
                            StructTable[f.name[0..f.name.indexOf('<')]].generateWithTemplate(f.name[f.name.indexOf('<')..$],_t.types.dup);
                            return generate();
                        }
                        Generator.error(loc,"Unknown mixin or function '"~f.name~"'!");
                    }

                    // Template function
                    Lexer l = new Lexer(f.name,1);
                    Parser p = new Parser(l.getTokens(),1,"(builtin)");
                    FuncTable[f.name[0..f.name.indexOf('<')]].generateWithTemplate(p.parseType().instanceof!TypeStruct.types,f.name);
                    return generate();
                }
                Generator.error(loc,"Unknown mixin or function '"~f.name~"'!");
            }

            // Generate mixin-call
            return MixinTable[f.name].generate();
        }
        if(f.name.into(Generator.LLVMFunctions)) {
            if(FuncTable[f.name].args.length != args.length && FuncTable[f.name].isVararg != true) {
                Generator.error(loc,"The number of arguments in the call does not match the signature!");
                exit(1);
                assert(0);
            }
            calledFunc = FuncTable[f.name];
            LLVMValueRef[] params = getParameters(FuncTable[f.name].isVararg,FuncTable[f.name].args);

            LLVMValueRef _val;

            if(Generator.LLVMFunctionTypes[f.name] != LLVMVoidTypeInContext(Generator.Context)) _val = LLVMBuildCall(
                Generator.Builder,
                Generator.LLVMFunctions[f.name],
                params.ptr,
                cast(uint)params.length,
                toStringz("CallFunc"~f.name)
            );
            _val = LLVMBuildCall(
                Generator.Builder,
                Generator.LLVMFunctions[f.name],
                params.ptr,
                cast(uint)params.length,
                toStringz("")
            );
            if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
            if(currScope.inTry) {
                if(LLVMGetTypeKind(LLVMTypeOf(_val)) == LLVMStructTypeKind) {
                    if((cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(_val)))).indexOf("std::error<") != -1) {
                        new NodeCall(loc,new NodeGet(new NodeDone(_val),"catch",true,loc),[]).generate();
                        _val = new NodeGet(new NodeDone(_val),"result",false,loc).generate();
                    }
                }
            }
            return _val;
        }
        LLVMValueRef _val;

        string rname = f.name;
        LLVMValueRef[] params;
        Type[] _types = getTypes();
        if(into(f.name~typesToString(_types),FuncTable)) {
            rname ~= typesToString(_types);
            if(rname !in Generator.Functions) {
                if(f.name.into(Generator.Functions)) {
                    calledFunc = FuncTable[f.name];
                    rname = f.name;
                }
                else Generator.error(loc,"Function '"~rname~"' does not exist!");
            }
            else calledFunc = FuncTable[rname];
        }
        else {
            if(rname !in Generator.Functions) {
                if(rname !in FuncTable) Generator.error(loc,"Function '"~rname~"' does not exist!");
                if(FuncTable[rname].isCtargs == true) {
                    if(!(rname~typesToString(_types)).into(FuncTable)) {
                        LLVMTypeRef[] Ltypes;
                        for(int i=0; i<_types.length; i++) {
                            Ltypes ~= Generator.GenerateType(_types[i],loc);
                        }
                        string _newname = FuncTable[rname].generateWithCtargs(Ltypes.dup);
                        rname = _newname;
                    }
                    else rname = rname~typesToString(_types);
                }
            }
            calledFunc = FuncTable[rname];
            params = getParameters(FuncTable[rname].isVararg, FuncTable[rname].args);
        }
        string name = "CallFunc"~f.name;
        if(FuncTable[rname].type.instanceof!TypeVoid) name = "";
        if(FuncTable[rname].args.length != args.length && FuncTable[rname].isVararg != true) {
            Generator.error(
                loc,
                "The number of arguments in the call of string "~rname~"("~to!string(args.length)~") does not match the signature("~to!string(FuncTable[rname].args.length)~")!"
            );
            exit(1);
            assert(0);
        }
        if(fromStringz(LLVMPrintValueToString(Generator.Functions[rname])).indexOf("llvm.") != -1) {
            LLVMValueRef _v = LLVMBuildAlloca(Generator.Builder,LLVMInt1TypeInContext(Generator.Context),toStringz("fix"));
            if(Generator.opts.optimizeLevel > 0) LLVMBuildCall(Generator.Builder,Generator.Functions["std::dontuse::_void"],[_v].ptr,1,toStringz(""));
            // This is necessary to solve a bug with LLVM-11
        }
        if(args.length != params.length) {
            params = getParameters(FuncTable[rname].isVararg, FuncTable[rname].args);
        }
        _val = LLVMBuildCall(
            Generator.Builder,
            Generator.Functions[rname],
            params.ptr,
            cast(uint)args.length,
            toStringz(name)
        );
        if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
        if(currScope.inTry) {
            if(LLVMGetTypeKind(LLVMTypeOf(_val)) == LLVMStructTypeKind) {
                if((cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(_val)))).indexOf("std::error<") != -1) {
                    new NodeCall(loc,new NodeGet(new NodeDone(_val),"catch",true,loc),[]).generate();
                    _val = new NodeGet(new NodeDone(_val),"result",false,loc).generate();
                }
            }
        }
        return _val;
        }
        else if(NodeGet g = func.instanceof!NodeGet) {
            LLVMValueRef _val;
            if(NodeIden i = g.base.instanceof!NodeIden) {
                TypeStruct s;
                if(TypePointer p = currScope.getVar(i.name,loc).t.instanceof!TypePointer) {
                    s = p.instance.instanceof!TypeStruct;
                }
                else s = currScope.getVar(i.name,loc).t.instanceof!TypeStruct;

                if(s is null) {
                    // Template
                    LLVMValueRef v = currScope.getWithoutLoad(i.name,loc);

                    if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) s = new TypeStruct(cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(v))));
                    else if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMPointerTypeKind) {
                        if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(v))) == LLVMStructTypeKind) {
                            s = new TypeStruct(cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(v)))));
                        }
                        else if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(v))) == LLVMPointerTypeKind) {
                            s = new TypeStruct(cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(v))))));
                        }
                    }
                }
                this.args = new NodeIden(i.name,loc)~this.args;

                LLVMValueRef _toCall = null;
                NodeVar v = null;
                TypeFunc t = null;

                if(s is null || g.field.into(FuncTable)) {
                    // Just call function as method of value
                    if(args.length != FuncTable[g.field].args.length) {
                        Generator.error(
                            loc,
                            "The number of arguments in the call of function '"~FuncTable[g.field].name~"'("~to!string(args.length)~") does not match the signature("~to!string(FuncTable[g.field].args.length)~")!"
                        );
                        exit(1);
                        assert(0);
                    }

                    calledFunc = FuncTable[g.field];
                    LLVMValueRef[] params = getParameters(FuncTable[g.field].isVararg, FuncTable[g.field].args);
                    _val = LLVMBuildCall(
                        Generator.Builder,
                        Generator.Functions[FuncTable[g.field].name],
                        params.ptr,
                        cast(uint)params.length,
                        toStringz(FuncTable[g.field].type.instanceof!TypeVoid ? "" : "call")
                    );
                    if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                }

                LLVMValueRef[] params;
                if(s.name.into(Generator.toReplace)) {
                    s = Generator.toReplace[s.name].instanceof!TypeStruct;
                }
                if(!into(s.name,StructTable) && s.name.indexOf('<') != -1) {
                    if(s.name.into(Generator.toReplace)) s = Generator.toReplace[s.name].instanceof!TypeStruct;
                    else {
                        Lexer l = new Lexer(s.name,1);
                        Parser p = new Parser(l.getTokens(),1,"(builtin_sname)");
                        TypeStruct ts = p.parseType().instanceof!TypeStruct;
                        for(int j=0; j<ts.types.length; j++) {
                            while(ts.types[j].toString().into(Generator.toReplace)) {
                                ts.types[j] = Generator.toReplace[ts.types[j].toString()];
                            }
                        }
                        ts.updateByTypes();
                        s = ts;
                    }
                }

                if(!into(cast(immutable)[s.name,g.field],MethodTable)) {
                    if(!cast(immutable)[s.name,g.field].into(structsNumbers)) {
                        Type[] pregenerate = getTypes();
                        string n;
                        if(pregenerate[0].instanceof!TypePointer) {
                            n = Generator.typeToString(Generator.GenerateType(pregenerate[0],loc))[1..$];
                        }
                        else {
                            n = cast(string)fromStringz(LLVMGetStructName(Generator.GenerateType(pregenerate[0],loc)));
                        }

                        if(g.field.indexOf('<') != -1) {
                            string newName = g.field[0..g.field.indexOf('<')];
                            Lexer l = new Lexer(g.field,1);
                            Parser p = new Parser(l.getTokens(),1,"(builtin_im)");
                            if(("{"~n~"*}"~newName).into(FuncTable)) {
                                TypeStruct ts = p.parseType().instanceof!TypeStruct;
                                FuncTable["{"~n~"*}"~newName].generateWithTemplate(ts.types,(ts.name));
                                return new NodeCall(loc,new NodeIden(("{"~n~"*}"~ts.name),loc),args).generate();
                            }
                            else if(("{"~n~"}"~newName).into(FuncTable)) {
                                TypeStruct ts = p.parseType().instanceof!TypeStruct;
                                FuncTable["{"~n~"}"~newName].generateWithTemplate(ts.types,(ts.name));
                                return new NodeCall(loc,new NodeIden(("{"~n~"}"~ts.name),loc),args).generate();
                            }
                            assert(0);
                        }

                        if(("{"~n~"}"~g.field).into(FuncTable) || ("{"~n~"*}"~g.field).into(FuncTable)) {
                            // Internal method
                            if(("{"~n~"*}"~g.field).into(FuncTable)) return new NodeCall(loc,new NodeIden(("{"~n~"*}"~g.field),loc),args).generate();
                            return new NodeCall(loc,new NodeIden(("{"~n~"}"~g.field),loc),args).generate();
                        }
                        foreach(k; byKey(FuncTable)) {
                            if(k.indexOf("pr") != -1) writeln(k);
                        }
                        Generator.error(loc,"Structure '"~s.name~"' doesn't contain method '"~g.field~"'!");
                    }
                    v = StructTable[s.name].elements[structsNumbers[cast(immutable)[s.name,g.field]].number].instanceof!NodeVar;
                    if(TypePointer vtp = v.t.instanceof!TypePointer) t = vtp.instance.instanceof!TypeFunc;
                    else t = v.t.instanceof!TypeFunc;

                    FuncArgSet[] _args;
                    for(int z=0; z<t.args.length; z++) {
                        _args ~= FuncArgSet(t.args[z].name,t.args[z].type);
                    }

                    if(args.length < 2) args = [];
                    else args = args[1..$];

                    params = getParameters(false,_args);
                    g.isMustBePtr = true;
                    _toCall = LLVMBuildLoad(Generator.Builder,Generator.byIndex(g.generate(),[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),structsNumbers[cast(immutable)[s.name,g.field]].number,false)]),toStringz("A"));
                }
                else {
                    calledFunc = MethodTable[cast(immutable)[s.name,g.field]];
                    params = getParameters(MethodTable[cast(immutable)[s.name,g.field]].isVararg, MethodTable[cast(immutable)[s.name,g.field]].args);
                }

                if(_toCall !is null) {
                    _val = LLVMBuildCall(
                        Generator.Builder,
                        _toCall,
                        params.ptr,
                        cast(uint)params.length,
                        toStringz(t.main.instanceof!TypeVoid ? "" : "call")
                    );
                    if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                    return _val;
                }
                if(cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))].into(MethodTable)) {
                    calledFunc = MethodTable[cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))]];
                    params = getParameters(MethodTable[cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))]].isVararg, MethodTable[cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))]].args);
                    _val = LLVMBuildCall(
                        Generator.Builder,
                        Generator.Functions[MethodTable[cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))]].name],
                        params.ptr,
                        cast(uint)params.length,
                        toStringz(MethodTable[cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))]].type.instanceof!TypeVoid ? "" : g.field)
                    );
                    if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                    return _val;
                }
                _val = LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[s.name,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[s.name,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
                if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                return _val;
            }
            else if(NodeIndex ni = g.base.instanceof!NodeIndex) {
                auto val = ni.generate();
                string sname = Generator.typeToString(LLVMTypeOf(val))[1..$-1];

                calledFunc = MethodTable[cast(immutable)[sname,g.field]];
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].isVararg, MethodTable[cast(immutable)[sname,g.field]].args);

                params = val~params;

                _val = LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
                if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                return _val;
            }
            else if(NodeGet ng = g.base.instanceof!NodeGet) {
                ng.isMustBePtr = true;
                auto val = ng.generate();
                string sname = "";
                if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMStructTypeKind) sname = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(val)));
                else sname = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(val))));

                calledFunc = MethodTable[cast(immutable)[sname,g.field]];
                _offset = 1;
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].isVararg, MethodTable[cast(immutable)[sname,g.field]].args);
                _offset = 0;

                params = val~params;

                _val = LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
                if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                return _val;
            }
            else if(NodeCast nc = g.base.instanceof!NodeCast) {
                LLVMValueRef v = nc.generate();
                if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) {
                    LLVMValueRef temp = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(v),toStringz("temp_"));
                    LLVMBuildStore(Generator.Builder,v,temp);
                    v = temp;
                }

                if(!(nc.type.instanceof!TypeStruct)) {
                    Generator.error(loc,"Type '"~nc.type.toString()~"' is not a structure!");
                }
                string sname = nc.type.instanceof!TypeStruct.name;
                calledFunc = MethodTable[cast(immutable)[sname,g.field]];
                _offset = 1;
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].isVararg, MethodTable[cast(immutable)[sname,g.field]].args);
                _offset = 0;

                params = v~params;
                _val = LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
                if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                return _val;
            }
            else if(NodeBuiltin nb = g.base.instanceof!NodeBuiltin) {
                LLVMValueRef v = nb.generate();
                string sname = "";

                if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) {
                    sname = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(v)));
                    LLVMValueRef temp = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(v),toStringz("temp_"));
                    LLVMBuildStore(Generator.Builder,v,temp);
                    v = temp;
                }
                else {
                    sname = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(v))));
                }
                if(!sname.into(StructTable)) {
                    Generator.error(loc,"Type '"~sname~"' is not a structure!");
                }
                calledFunc = MethodTable[cast(immutable)[sname,g.field]];
                _offset = 1;
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].isVararg, MethodTable[cast(immutable)[sname,g.field]].args);
                _offset = 0;

                params = v~params;
                _val = LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
                if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                return _val;
            }
            else if(NodeDone nd = g.base.instanceof!NodeDone) {
                LLVMValueRef v = nd.generate();
                string sname = "";

                if(LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMStructTypeKind) {
                    sname = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(v)));
                    LLVMValueRef temp = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(v),toStringz("temp_"));
                    LLVMBuildStore(Generator.Builder,v,temp);
                    v = temp;
                }
                else assert(0);

                if(!sname.into(StructTable)) {
                    Generator.error(loc,"Type '"~sname~"' is not a structure!");
                }
                calledFunc = MethodTable[cast(immutable)[sname,g.field]];
                _offset = 1;
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].isVararg, MethodTable[cast(immutable)[sname,g.field]].args);
                _offset = 0;

                params = v~params;
                _val = LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
                if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
                return _val;
            }
            else {
                writeln(g.base,"1");
                Generator.error(loc,"TODO!");
            }
        }
        else if(NodeUnary un = func.instanceof!NodeUnary) {
            NodeCall nc = new NodeCall(loc,un.base,args.dup);
            if(un.type == TokType.Ne) nc._inverted = true;
            return nc.generate();
        }
        writeln(func,"2");
        Generator.error(loc,"TODO!");
        assert(0);
    }


    override void debugPrint() {
        writeln("NodeCall()");
    }
}

class NodeIndex : Node {
    Node element;
    Node[] indexs;
    int loc;
    bool isMustBePtr = false;
    bool elementIsConst = false;

    this(Node element, Node[] indexs, int loc) {
        this.element = element;
        this.indexs = indexs.dup;
        this.loc = loc;
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) element.check();
    }

    override Type getType() {
        Type elType = element.getType();
        if(!elType.instanceof!TypePointer && !elType.instanceof!TypeArray) {
            if(TypeStruct ts = elType.instanceof!TypeStruct) {
                if(TokType.Rbra.into(StructTable[ts.name].operators)) return StructTable[ts.name].operators[TokType.Rbra][""].type;
            } 
            assert(0);
        }

        if(TypePointer tp = elType.instanceof!TypePointer) return tp.instance;
        else if(TypeArray ta = elType.instanceof!TypeArray) return ta.element;
        assert(0);
    }

    LLVMValueRef[] generateIndexs() {
        pragma(inline,true);
        LLVMValueRef[] _indexs;

        foreach(Node ind; indexs) {
            _indexs ~= ind.generate();
        }

        return _indexs;
    }

    bool isElementConst(Type _t) {
        Type currT = _t;

        while(currT.instanceof!TypeConst) currT = currT.instanceof!TypeConst.instance;

        if(TypePointer tp = currT.instanceof!TypePointer) {
            if(tp.instance.instanceof!TypeConst) return true;
            return false;
        }

        if(TypeArray ta = currT.instanceof!TypeArray) {
            if(ta.element.instanceof!TypeConst) return true;
            return false;
        }

        return false;
    }

    override LLVMValueRef generate() {
        if(NodeIden id = element.instanceof!NodeIden) {
            Type _t = currScope.getVar(id.name,loc).t;

            if(TypePointer tp = _t.instanceof!TypePointer) {
                if(tp.instance.instanceof!TypeConst) elementIsConst = true;
            }
            else if(TypeArray ta = _t.instanceof!TypeArray) {
                if(ta.element.instanceof!TypeConst) elementIsConst = true;
            }
            else if(TypeConst tc = _t.instanceof!TypeConst) {
                elementIsConst = isElementConst(tc.instance);
            }

            LLVMValueRef ptr = currScope.get(id.name,loc);
            if(Generator.opts.runtimeChecks && LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind && !FuncTable[currScope.func].isNoChecks) {
                LLVMValueRef isNull = LLVMBuildICmp(
                    Generator.Builder,
                    LLVMIntNE,
                    LLVMBuildPtrToInt(Generator.Builder,ptr,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                    LLVMBuildPtrToInt(Generator.Builder,new NodeNull().generate(),LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                    toStringz("assert(p==null)_")
                );
                if(NeededFunctions["assert"].into(Generator.Functions)) LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: attempt to use a null pointer in ptoi!\n",false).generate()].ptr,2,toStringz(""));
            }
            if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMArrayTypeKind && LLVMGetTypeKind(LLVMTypeOf(currScope.getWithoutLoad(id.name,loc))) == LLVMArrayTypeKind) {
                // Argument
                LLVMValueRef copyVal = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(ptr),toStringz("copyVal_"));
                LLVMBuildStore(Generator.Builder,ptr,copyVal);
                ptr = copyVal;
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(ptr)) != LLVMPointerTypeKind) ptr = currScope.getWithoutLoad(id.name,loc);

            string structName = "";
            if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMStructTypeKind) structName = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(ptr)));
            else if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind) {
                if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMStructTypeKind) {
                    structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
                }
                else if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMPointerTypeKind) {
                    if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMStructTypeKind) {
                        structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(ptr)))));
                    }
                }
            }

            if(structName != "" && TokType.Rbra.into(StructTable[structName].operators)) {
                NodeFunc _f = StructTable[structName].operators[TokType.Rbra][""];
                if(_f.args[0].type.instanceof!TypeStruct) {
                    if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind) ptr = LLVMBuildLoad(Generator.Builder,ptr,toStringz("load_"));
                }
                return LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[_f.name],
                    [ptr, indexs[0].generate()].ptr,
                    2,
                    toStringz("call")
                );
            }

            LLVMValueRef index = Generator.byIndex(ptr,generateIndexs());

            if(isMustBePtr) return index;
            return LLVMBuildLoad(Generator.Builder,index,toStringz("load1684_"));
        }
        if(NodeGet get = element.instanceof!NodeGet) {
            get.isMustBePtr = true;
            LLVMValueRef ptr = get.generate();

            elementIsConst = get.elementIsConst;

            if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) != LLVMArrayTypeKind) {ptr = LLVMBuildLoad(Generator.Builder,ptr,toStringz("load1852_"));}
            LLVMValueRef index = Generator.byIndex(ptr,generateIndexs());

            if(isMustBePtr) return index;
            return LLVMBuildLoad(Generator.Builder,index,toStringz("load1691_"));
        }
        if(NodeCall nc = element.instanceof!NodeCall) {
            LLVMValueRef vr = nc.generate();

            LLVMValueRef index = Generator.byIndex(vr,generateIndexs());
            if(isMustBePtr) return index;
            return LLVMBuildLoad(Generator.Builder,index,toStringz("load2062_"));
        }
        if(NodeDone nd = element.instanceof!NodeDone) {
            LLVMValueRef val = nd.generate();

            LLVMValueRef index = Generator.byIndex(val,generateIndexs());
            if(isMustBePtr) return index;
            return LLVMBuildLoad(Generator.Builder,index,toStringz("load3013_"));
        }
        if(NodeCast nc = element.instanceof!NodeCast) {
            //nc.isMustBePtr = true;
            LLVMValueRef val = nc.generate();

            LLVMValueRef index = Generator.byIndex(val,generateIndexs());
            if(isMustBePtr) return index;
            return LLVMBuildLoad(Generator.Builder,index,toStringz("load3013_"));
        }
        writeln("Base: ",element);
        assert(0);
    }
}

StructMember[string[]] structsNumbers;

class NodeGet : Node {
    Node base;
    string field;
    int loc;
    bool isMustBePtr;
    bool elementIsConst = false;

    this(Node base, string field, bool isMustBePtr, int loc) {
        this.base = base;
        this.field = field;
        this.loc = loc;
        this.isMustBePtr = isMustBePtr;
    }

    override Type getType() {
        Type _t = base.getType();
        TypeStruct ts;
        if(!_t.instanceof!TypeStruct) {
            if(!_t.instanceof!TypePointer) Generator.error(loc,"Structure '"~_t.toString()~"' doesn't exist! (getType)");
            TypePointer tp = _t.instanceof!TypePointer;
            ts = tp.instance.instanceof!TypeStruct;
        }
        else ts = _t.instanceof!TypeStruct;

        if(ts is null) {
            Generator.error(loc,"Type '"~_t.toString()~"' is not a structure!");
        }

        if(ts.name.into(Generator.toReplace)) {
            while(ts.toString().into(Generator.toReplace)) {
                ts = Generator.toReplace[ts.toString()].instanceof!TypeStruct;
            }
        }

        if(into(cast(immutable)[ts.name,field],MethodTable)) {
            return MethodTable[cast(immutable)[ts.name,field]].getType();
        }
        if(into(cast(immutable)[ts.name,field],structsNumbers)) return structsNumbers[cast(immutable)[ts.name,field]].var.getType();
        foreach(k; byKey(Generator.toReplace)) {
            if(k.indexOf('<') != -1) return Generator.toReplace[k];
        }
        Generator.error(loc,"Structure '"~ts.name~"' doesn't contain element "~field~"!");
        assert(0);
    }

    LLVMValueRef checkStructure(LLVMValueRef ptr) {
        if(Generator.typeToString(LLVMTypeOf(ptr))[$-1..$] != "*") {
            LLVMValueRef temp = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(ptr),toStringz("temp_"));
            LLVMBuildStore(Generator.Builder,ptr,temp);
            return temp;
        }
        while(Generator.typeToString(LLVMTypeOf(ptr))[$-2..$] == "**") ptr = LLVMBuildLoad(Generator.Builder,ptr,toStringz("load1691_"));
        return ptr;
    }

    LLVMValueRef checkIn(string struc) {
        if(!into(struc,StructTable)) {
            Generator.error(loc, "Structure '"~struc~"' doesn't exist!");
            return null;
        }
        if(!into(cast(immutable)[struc,field],structsNumbers)) {
            if(into(cast(immutable)[struc,field],MethodTable)) {
                return Generator.Functions[MethodTable[cast(immutable)[struc,field]].name];
            }
            else Generator.error(loc,"Structure '"~struc~"' doesn't contain element "~field~"!");
        }
        if(structsNumbers[cast(immutable)[struc,field]].var.t.instanceof!TypeConst) elementIsConst = true;
        return null;
    }

    void checkIsNull(LLVMValueRef ptr) {
        if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind && Generator.opts.runtimeChecks && !FuncTable[currScope.func].isNoChecks) {
            LLVMValueRef isNull = LLVMBuildICmp(
                Generator.Builder,
                LLVMIntNE,
                LLVMBuildPtrToInt(Generator.Builder,ptr,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                LLVMBuildPtrToInt(Generator.Builder,new NodeNull().generate(),LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                toStringz("assert(p==null)_")
            );
            LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: attempt to get an element from a null pointer to a structure!\n",false).generate()].ptr,2,toStringz(""));
        }
    }

    override LLVMValueRef generate() {
        if(NodeIden id = base.instanceof!NodeIden) {
            LLVMValueRef ptr = checkStructure(currScope.getWithoutLoad(id.name,loc));
            string structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
            LLVMValueRef f = checkIn(structName);
            if(f !is null) return f;

            if(isMustBePtr) return LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                cast(uint)structsNumbers[cast(immutable)[structName,field]].number,
                toStringz("sgep1700_")
            );
            return LLVMBuildLoad(Generator.Builder, LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                cast(uint)structsNumbers[cast(immutable)[structName,field]].number,
                toStringz("sgep1727_")
            ),toStringz("load1727_"));
        }
        if(NodeIndex ind = base.instanceof!NodeIndex) {
            ind.isMustBePtr = true;

            LLVMValueRef ptr = checkStructure(ind.generate());
            string structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
            LLVMValueRef f = checkIn(structName);

            if(f !is null) return f;

            if(isMustBePtr) return LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                cast(uint)structsNumbers[cast(immutable)[structName,field]].number,
                toStringz("sgep1739_")
            );
            return LLVMBuildLoad(Generator.Builder,LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                cast(uint)structsNumbers[cast(immutable)[structName,field]].number,
                toStringz("sgep1745_")
            ), toStringz("load1745_"));
        }
        if(NodeGet ng = base.instanceof!NodeGet) {
            ng.isMustBePtr = true;

            LLVMValueRef ptr = checkStructure(ng.generate());
            string structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
            LLVMValueRef f = checkIn(structName);

            if(f !is null) return f;

            if(isMustBePtr) return LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                cast(uint)structsNumbers[cast(immutable)[structName,field]].number,
                toStringz("sgep1739_")
            );
            return LLVMBuildLoad(Generator.Builder,LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                cast(uint)structsNumbers[cast(immutable)[structName,field]].number,
                toStringz("sgep1745_")
            ), toStringz("load1745_"));
        }
        if(NodeBuiltin nb = base.instanceof!NodeBuiltin) {
            LLVMValueRef v = nb.generate();

            string structName;
            if(v is null) {
                Generator.error(loc, "The built-in function '"~nb.name~"' does not return values!");
            }

            LLVMValueRef ptr = checkStructure(v);
            structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
            LLVMValueRef f = checkIn(structName);

            if(f !is null) return f;

            if(isMustBePtr) return LLVMBuildStructGEP(
                Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
            );
            return LLVMBuildLoad(Generator.Builder, LLVMBuildStructGEP(
                Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
            ), toStringz("load1760_"));
        }
        if(NodeDone nd = base.instanceof!NodeDone) {
            LLVMValueRef done = nd.generate();
            string structName = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(done)));

            LLVMValueRef ptr = checkStructure(done);
            LLVMValueRef f = checkIn(structName);

            if(f !is null) return f;

            if(isMustBePtr) return LLVMBuildStructGEP(
                Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
            );
            return LLVMBuildLoad(Generator.Builder, LLVMBuildStructGEP(
                Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
            ), toStringz("load1780_"));
        }
        writeln("Base: ",base);
        assert(0);
    }
}

class NodeBool : Node {
    bool value;

    this(bool value) {
        this.value = value;
    }

    override Type getType() {
        return new TypeBasic("bool");
    }

    override void check() {
        this.isChecked = true;
    }

    override LLVMValueRef generate() {
        return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),to!ulong(value),false);
    }
}

class NodeUnary : Node {
    int loc;
    TokType type;
    Node base;
    bool isEqu = false;

    this(int loc, TokType type, Node base) {
        this.loc = loc;
        this.type = type;
        this.base = base;
    }

    override Type getType() {
        switch(type) {
            case TokType.GetPtr: return new TypePointer(base.getType());
            case TokType.Minus: return base.getType();
            case TokType.Ne: return base.getType();
            case TokType.Destructor: return new TypeVoid();
            case TokType.Multiply:
                Type _t = base.getType();
                if(TypePointer tp = _t.instanceof!TypePointer) return tp.instance;
                else if(TypeArray ta = _t.instanceof!TypeArray) return ta.element;
                assert(0);
            default: assert(0);
        }
    }

    LLVMValueRef generatePtr() {
        if(NodeIden id = base.instanceof!NodeIden) {
            id.isMustBePtr = true;
            return id.generate();
        }
        if(NodeIndex ind = base.instanceof!NodeIndex) {
            ind.isMustBePtr = true;
            return ind.generate();
        }
        if(NodeGet ng = base.instanceof!NodeGet) {
            ng.isMustBePtr = true;
            return ng.generate();
        }
        assert(0);
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) base.check();
    }
    override LLVMValueRef generate() {
        if(type == TokType.Minus) {
            LLVMValueRef bs = base.generate();
            if(LLVMGetTypeKind(LLVMTypeOf(bs)) == LLVMIntegerTypeKind) return LLVMBuildNeg(
                Generator.Builder,
                bs,
                toStringz("neg")
            );
            return LLVMBuildFNeg(
                Generator.Builder,
                bs,
                toStringz("fneg")
            );
        }
        else if(type == TokType.GetPtr) {
            LLVMValueRef val;
            if(NodeGet s = base.instanceof!NodeGet) {
                s.isMustBePtr = true;
                val = s.generate();
            }
            else if(NodeCall call = base.instanceof!NodeCall) {
                auto gcall = call.generate();
                auto temp = LLVMBuildAlloca(
                    Generator.Builder,
                    LLVMTypeOf(gcall),
                    toStringz("temp")
                );
                LLVMBuildStore(
                    Generator.Builder,
                    gcall,
                    temp
                );
                val = temp;
            }
            else if(NodeBinary bin = base.instanceof!NodeBinary) {
                LLVMValueRef v = bin.generate();
                val = Generator.byIndex(v,[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),0,false)]);
            }
            else if(NodeSlice sl = base.instanceof!NodeSlice) {
                sl.isMustBePtr = true;
                val = sl.generate();
            }
            else if(NodeIden id = base.instanceof!NodeIden) {
                if(TypeArray ta = currScope.getVar(id.name,loc).t.instanceof!TypeArray) {
                    LLVMValueRef ptr = LLVMBuildInBoundsGEP(
			    		Generator.Builder,
			    		currScope.getWithoutLoad(id.name),
			    		[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
			    		2,
			    		toStringz("ingep")
			    	);
                    val = ptr;
                }
                else val = currScope.getWithoutLoad(id.name);
            }
            else if(NodeIndex ind = base.instanceof!NodeIndex) {
                ind.isMustBePtr = true;
                val = ind.generate();
            }
            else assert(0);
            if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val))) == LLVMArrayTypeKind) {
                return LLVMBuildPointerCast(
                    Generator.Builder,
                    val,
                    LLVMPointerType(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(val))),0),
                    toStringz("bitcast")
                );
            }
            else return val;
        }
        else if(type == TokType.Ne) {
            return LLVMBuildNot(Generator.Builder,base.generate(),toStringz("not_"));
        }
        else if(type == TokType.Multiply) {
            LLVMValueRef _base = base.generate();
            if(Generator.opts.runtimeChecks) {
                LLVMValueRef isNull = LLVMBuildICmp(
                    Generator.Builder,
                    LLVMIntNE,
                    LLVMBuildPtrToInt(Generator.Builder,_base,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                    LLVMBuildPtrToInt(Generator.Builder,new NodeNull().generate(),LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                    toStringz("assert(p==null)_")
                );
                if(NeededFunctions["assert"].into(Generator.Functions)) LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: attempt to use a null pointer in ptoi!\n",false).generate()].ptr,2,toStringz(""));
            }
            return LLVMBuildLoad(
                Generator.Builder,
                _base,
                toStringz("load1958_")
            );
        }
        else if(type == TokType.Destructor) {
            LLVMValueRef val = generatePtr();

            if(LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMPointerTypeKind && LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMStructTypeKind) {
                Generator.error(loc,"the attempt to call the destructor isn't in the structure!");
            }

            if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) {
                if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val))) == LLVMPointerTypeKind) {
                    if(LLVMGetTypeKind(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(val)))) == LLVMStructTypeKind) {
                        val = LLVMBuildLoad(Generator.Builder,val,toStringz("load2207_"));
                    }
                }
            }

            string struc = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(val))));

            if(StructTable[struc]._destructor is null) {
                if(NodeIden id = base.instanceof!NodeIden) {
                    if(!into(id.name,currScope.localVars) && !currScope.localVars[id.name].t.instanceof!TypeStruct) {
                        return new NodeCall(loc,new NodeIden(NeededFunctions["free"],loc),[base]).generate();
                    }
                    else return null;
                }
                return new NodeCall(loc,new NodeIden(NeededFunctions["free"],loc),[base]).generate();
            }
            return new NodeCall(loc,new NodeIden(StructTable[struc]._destructor.name,loc),[base]).generate();
        }
        assert(0);
    }

    override Node comptime() {
        if(type == TokType.Ne) return new NodeBool(!base.comptime().instanceof!NodeBool.value);
        else Generator.error(loc,"Using the unary operator '"~to!string(type)~" during compilation is not supported!");
        assert(0);
    }
}

class NodeIf : Node {
    Node condition;
    Node _body;
    Node _else;
    int loc;
    string func;
    bool isStatic = false;
    bool[2] _hasRets;

    this(Node condition, Node _body, Node _else, int loc, string func, bool isStatic) {
        this.condition = condition;
        this._body = _body;
        this._else = _else;
        this.loc = loc;
        this.func = func;
    }

    override Type getType() {
        return new TypeVoid();
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) {
            if(_body !is null) {
                if(NodeRet r = _body.instanceof!NodeRet) {
                    r.parent = func;
                    _hasRets[0] = true;
                }
                else if(NodeBlock b = _body.instanceof!NodeBlock) {
                    foreach(n; b.nodes) {
                        if(NodeRet r = n.instanceof!NodeRet) {
                            r.parent = func;
                            _hasRets[0] = true;
                        }
                    }
                }
            }

            if(_else !is null) {
                if(NodeRet r = _else.instanceof!NodeRet) {
                    r.parent = func;
                    _hasRets[1] = true;
                }
                else if(NodeBlock b = _else.instanceof!NodeBlock) {
                    foreach(n; b.nodes) {
                        if(NodeRet r = n.instanceof!NodeRet) {
                            r.parent = func;
                            _hasRets[1] = true;
                        }
                    }
                }
            }

            condition.check();
            if(_body !is null) _body.check();
            if(_else !is null) _else.check();
        }
    }

    override Node comptime() {
        NodeBool b = condition.comptime().instanceof!NodeBool;
        if(b.value && _body !is null) {
            _body.check();
            _body.generate();
        }
        if(!b.value && _else !is null) {
            _else.check();
            _else.generate();
        }
        return null;
    }

    override LLVMValueRef generate() {
        if(isStatic) {
            comptime();
            return null;
        }
        LLVMBasicBlockRef thenBlock =
			LLVMAppendBasicBlockInContext(
                Generator.Context,
				Generator.Functions[currScope.func],
				toStringz("then"));

		LLVMBasicBlockRef elseBlock =
			LLVMAppendBasicBlockInContext(
                Generator.Context,
				Generator.Functions[currScope.func],
				toStringz("else"));

		LLVMBasicBlockRef endBlock =
			LLVMAppendBasicBlockInContext(
                Generator.Context,
				Generator.Functions[currScope.func],
				toStringz("end"));

        LLVMBuildCondBr(
			Generator.Builder,
			condition.generate(),
			thenBlock,
			elseBlock
		);

        int selfNum = cast(int)Generator.ActiveLoops.length;
        Generator.ActiveLoops[selfNum] = Loop(true,thenBlock,endBlock,false,true);

        LLVMPositionBuilderAtEnd(Generator.Builder, thenBlock);
        Generator.currBB = thenBlock;
        if(_body !is null) _body.generate();
        if(!Generator.ActiveLoops[selfNum].hasEnd) LLVMBuildBr(Generator.Builder,endBlock);

        if(Generator.ActiveLoops[selfNum].rets.length>0) {
            FuncTable[func].rets ~= Generator.ActiveLoops[selfNum].rets[0].ret;
        }

        bool hasEnd1 = Generator.ActiveLoops[selfNum].hasEnd;

        Generator.ActiveLoops[selfNum] = Loop(true,elseBlock,endBlock,false,true);

        LLVMPositionBuilderAtEnd(Generator.Builder, elseBlock);
		Generator.currBB = elseBlock;
        if(_else !is null) _else.generate();
        if(!Generator.ActiveLoops[selfNum].hasEnd) LLVMBuildBr(Generator.Builder,endBlock);

        bool hasEnd2 = Generator.ActiveLoops[selfNum].hasEnd;

        if(Generator.ActiveLoops[selfNum].rets.length>0) {
            FuncTable[func].rets ~= Generator.ActiveLoops[selfNum].rets[0].ret;
        }

        LLVMPositionBuilderAtEnd(Generator.Builder, endBlock);
        Generator.currBB = endBlock;
        Generator.ActiveLoops.remove(selfNum);

        if(hasEnd1 && hasEnd2) {
            if(Generator.ActiveLoops.length == 0) {
                LLVMBuildRet(Generator.Builder,LLVMConstNull(Generator.GenerateType(FuncTable[func].type,loc)));
                return null;
            }
        }

        return null;
    }
}

class NodeWhile : Node {
    Node condition;
    Node _body;
    string func;
    int loc;

    this(Node condition, Node _body, int loc, string func) {
        this.condition = condition;
        this._body = _body;
        this.loc = loc;
        this.func = func;
    }

    override Type getType() {
        return new TypeVoid();
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(_body !is null && !oldCheck) {
            if(NodeRet r = _body.instanceof!NodeRet) {
                r.parent = func;
            }
            else if(NodeBlock b = _body.instanceof!NodeBlock) {
                foreach(n; b.nodes) {
                    if(NodeRet r = n.instanceof!NodeRet) r.parent = func;
                }
            }
        }
    }

    override LLVMValueRef generate() {
        LLVMBasicBlockRef condBlock =
            LLVMAppendBasicBlock(
                Generator.Functions[currScope.func],
                toStringz("cond")
        );

        LLVMBasicBlockRef whileBlock =
            LLVMAppendBasicBlock(
                Generator.Functions[currScope.func],
                toStringz("while")
        );

        currScope.BlockExit = LLVMAppendBasicBlock(
            Generator.Functions[currScope.func],
            toStringz("exit")
        );

        LLVMBuildBr(
            Generator.Builder,
            condBlock
        );

        LLVMPositionBuilderAtEnd(Generator.Builder, condBlock);

        LLVMBuildCondBr(
            Generator.Builder,
            condition.generate(),
            whileBlock,
            currScope.BlockExit
        );

        LLVMPositionBuilderAtEnd(Generator.Builder, whileBlock);
        int selfNumber = cast(int)Generator.ActiveLoops.length;
        Generator.ActiveLoops[selfNumber] = Loop(true,condBlock,currScope.BlockExit,false,false);

        Generator.currBB = whileBlock;
        _body.generate();
        if(!Generator.ActiveLoops[selfNumber].hasEnd) LLVMBuildBr(
            Generator.Builder,
            condBlock
        );

        LLVMPositionBuilderAtEnd(Generator.Builder, Generator.ActiveLoops[selfNumber].end);
        Generator.ActiveLoops.remove(selfNumber);
        return null;
    }
}

class NodeBreak : Node {
    int loc;

    this(int loc) {this.loc = loc;}

    int getWhileLoop() {
        int i = cast(int)Generator.ActiveLoops.length-1;

        while((i>=0) && Generator.ActiveLoops[i].isIf) i -= 1;

        return i;
    }

    override Type getType() {
        return new TypeVoid();
    }

    override void check() {
        this.isChecked = true;
    }
    override LLVMValueRef generate() {
        if(Generator.ActiveLoops.length == 0) Generator.error(loc,"Attempt to call 'break' out of the loop!");
        int id = getWhileLoop();

        Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].hasEnd = true;

        LLVMBuildBr(
            Generator.Builder,
            Generator.ActiveLoops[id].end
        );
        return null;
    }
}

class NodeContinue : Node {
    int loc;

    this(int loc) {this.loc = loc;}

    int getWhileLoop() {
        int i = cast(int)Generator.ActiveLoops.length-1;

        while((i>=0) && Generator.ActiveLoops[i].isIf) {
            i -= 1;
        }

        return i;
    }

    override Type getType() {
        return new TypeVoid();
    }

    override void check() {
        this.isChecked = true;
    }
    override LLVMValueRef generate() {
        if(Generator.ActiveLoops.length == 0) Generator.error(loc,"Attempt to call 'continue' out of the loop!");
        int id = getWhileLoop();

        Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].hasEnd = true;

        LLVMBuildBr(
            Generator.Builder,
            Generator.ActiveLoops[id].start
        );
        return null;
    }
}

class NodeNamespace : Node {
    string[] names;
    Node[] nodes;
    int loc;
    bool isImport = false;
    bool hidePrivated = false;

    this(string name, Node[] nodes, int loc) {
        this.names ~= name;
        this.nodes = nodes.dup;
        this.loc = loc;
    }

    override Node copy() {
        Node[] _nodes;
        for(int i=0; i<nodes.length; i++) if(nodes[i] !is null) _nodes ~= nodes[i].copy();
        return new NodeNamespace(names[0], _nodes, loc); 
    }

    override Type getType() {
        return new TypeVoid();
    }

    override LLVMValueRef generate() {
        if(currScope !is null) {
            Generator.error(loc,"Namespaces cannot be inside functions!");
            return null;
        }
        for(int i=0; i<nodes.length; i++) {
            if(NodeFunc f = nodes[i].instanceof!NodeFunc) {
                if(hidePrivated && f.isPrivate) continue;
                if(!f.isChecked) {
                    f.namespacesNames ~= names;
                    f.check();
                }
                if(!f.isExtern) f.isExtern = isImport;
                f.generate();
            }
            else if(NodeVar v = nodes[i].instanceof!NodeVar) {
                if(hidePrivated && v.isPrivate) continue;
                if(!v.isChecked) {
                    v.namespacesNames ~= names;
                    v.check();
                }
                if(!v.isExtern) v.isExtern = isImport;
                v.generate();
            }
            else if(NodeNamespace n = nodes[i].instanceof!NodeNamespace) {
                if(!n.isChecked) {
                    n.names ~= names;
                    n.check();
                }
                if(!n.isImport) n.isImport = isImport;
                n.generate();
            }
            else if(NodeStruct s = nodes[i].instanceof!NodeStruct) {
                if(!s.isChecked) {
                    s.namespacesNames ~= names;
                    s.check();
                }
                if(!s.isImported) s.isImported = isImport;
                s.generate();
            }
            else if(NodeAliasType nat = nodes[i].instanceof!NodeAliasType) {
                if(!nat.isChecked) {
                    nat.namespacesNames ~= names;
                    nat.check();
                }
                nat.generate();
            }
            else nodes[i].generate();
        }
        return null;
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) for(int i=0; i<nodes.length; i++) {
            if(NodeFunc f = nodes[i].instanceof!NodeFunc) {
                if(hidePrivated && f.isPrivate) continue;
                f.namespacesNames ~= names;
                f.check();
            }
            else if(NodeNamespace n = nodes[i].instanceof!NodeNamespace) {
                n.names ~= names;
                n.check();
            }
            else if(NodeVar v = nodes[i].instanceof!NodeVar) {
                if(hidePrivated && v.isPrivate) continue;
                v.namespacesNames ~= names;
                v.check();
            }
            else if(NodeStruct s = nodes[i].instanceof!NodeStruct) {
                s.namespacesNames ~= names;
                s.check();
            }
            else if(NodeMacro m = nodes[i].instanceof!NodeMacro) {
                m.namespacesNames ~= names;
                m.check();
            }
            else if(NodeAliasType nat = nodes[i].instanceof!NodeAliasType) {
                nat.namespacesNames ~= names;
                nat.check();
            }
        }
    }
}

struct StructPredefined {
    int element;
    Node value;
    bool isStruct;
}

class NodeStruct : Node {
    string name;
    Node[] elements;
    StructPredefined[] predefines;
    string[] namespacesNames;
    int loc;
    string origname;
    NodeFunc[] _this;
    NodeFunc _destructor;
    NodeFunc _with;
    NodeFunc[] methods;
    bool isImported = false;
    string _extends;
    NodeFunc[string][TokType] operators;
    Node[] _oldElements;
    string[] templateNames;
    bool noCompile = false;
    NodeFunc[] _thisFs;
    bool isComdat = false;
    DeclMod[] mods;
    bool isPacked = false;

    this(string name, Node[] elements, int loc, string _exs, string[] templateNames, DeclMod[] mods) {
        this.name = name;
        this._oldElements = elements.dup;
        this.elements = elements.dup;
        this.loc = loc;
        this.origname = name;
        this._extends = _exs;
        this.templateNames = templateNames.dup;
        this.mods = mods;
    }

    override Node copy() {
        Node[] _elements;
        for(int i=0; i<elements.length; i++) if(elements[i] !is null) _elements ~= elements[i].copy();
        return new NodeStruct(origname, _elements, loc, _extends, templateNames.dup, mods.dup);
    }

    LLVMTypeRef asConstType() {
        LLVMTypeRef[] types = getParameters(false);
        return LLVMStructType(types.ptr, cast(uint)types.length, false);
    }

    override Type getType() {
        return new TypeVoid();
    }

    int getSize() {
        int allSize = 0;
        for(int i=0; i<elements.length; i+=1) {
            if(NodeVar nv = elements[i].instanceof!NodeVar) {
                allSize += nv.t.getSize();
            }
        }
        return allSize;
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) {
            if(namespacesNames.length>0) {
                name = namespacesNamesToString(namespacesNames,name);
            }
            if(!(name !in StructTable)) {
                //checkError(loc,"a struct with that name('"~name~"') already exists on "~to!string(StructTable[name].loc+1)~" line!");
                noCompile = true;
                return;
            }
            if(_extends != "") {
                NodeStruct _s = StructTable[_extends];
                elements ~= _s.elements;
                methods ~= _s.methods;
                predefines ~= _s.predefines;
            }
            StructTable[name] = this;
        }
    }

    bool hasPredefines() {
        for(int i=0; i<elements.length; i++) {
            if(NodeVar v = elements[i].instanceof!NodeVar) {
                if(TypeStruct s = v.t.instanceof!TypeStruct) {
                    if(s.name != name && StructTable[s.name].hasPredefines()) return true;
                }
                else {
                    if(v.value !is null) return true;
                }
            }
        }
        return false;
    }

    LLVMTypeRef[] getParameters(bool isLinkOnce) {
        LLVMTypeRef[] values;
        for(int i=0; i<elements.length; i++) {
            if(NodeVar v = elements[i].instanceof!NodeVar) {
                structsNumbers[cast(immutable)[name,v.name]] = StructMember(i,v);
                if(v.value !is null) {
                    predefines ~= StructPredefined(i,v.value,false);
                }
                else if(TypeStruct ts = v.t.instanceof!TypeStruct) {
                    if(!(ts.name !in StructTable) && StructTable[ts.name].hasPredefines()) predefines ~= StructPredefined(i,v,true);
                }
                else predefines ~= StructPredefined(i,null,false);
                values ~= Generator.GenerateType(v.t,loc);
            }
            else {
                NodeFunc f = elements[i].instanceof!NodeFunc;
                if(f.isCtargs) continue;
                if(f.origname == "this") {
                    _this ~= f;
                    _this[_this.length-1].name = origname;
                    _this[_this.length-1].type = f.type;
                    _this[_this.length-1].namespacesNames = namespacesNames.dup;
                    _this[_this.length-1].isTemplatePart = isLinkOnce;
                    _this[_this.length-1].isComdat = isComdat;

                    if(_this[_this.length-1].args.length>0 && _this[_this.length-1].args[0].name == "this") _this[_this.length-1].args = _this[_this.length-1].args[1..$];

                    _thisFs ~= f;

                    if(isImported) {
                        _this[_this.length-1].isExtern = true;
                        _this[_this.length-1].check();
                        continue;
                    }

                    _this[_this.length-1].isExtern = false;

                    Node[] toAdd;
                    if((!f.type.instanceof!TypeStruct) && !canFind(f.mods,DeclMod("noNew",""))) {
                        toAdd ~= new NodeBinary(
                            TokType.Equ,
                            new NodeIden("this",_this[_this.length-1].loc),
                            new NodeCast(new TypePointer(new TypeStruct(name)),
                                new NodeCall(
                                    _this[_this.length-1].loc,new NodeIden(NeededFunctions["malloc"],_this[_this.length-1].loc),
                                    [new NodeCast(new TypeBasic("int"), new NodeSizeof(new NodeType(new TypeStruct(name),_this[_this.length-1].loc),_this[_this.length-1].loc), _this[_this.length-1].loc)]
                                ),
                                _this[_this.length-1].loc
                            ),
                            _this[_this.length-1].loc
                        );
                    }

                    _this[_this.length-1].block.nodes = new NodeVar("this",null,false,false,false,[],_this[_this.length-1].loc,f.type)~toAdd~_this[_this.length-1].block.nodes;
                    _this[_this.length-1].check();
                }
                else if(f.origname == name) {
                    Type outType = new TypePointer(new TypeStruct(name));
                    f.isTemplatePart = isLinkOnce;
                    f.isComdat = isComdat;
                    if(_this !is null) {
                        outType = _this[0].type;
                        if(outType.instanceof!TypeStruct) outType = new TypePointer(outType);
                    }
                    if((f.args.length > 0 && f.args[0].name != "this") || f.args.length == 0) f.args = FuncArgSet("this",outType)~f.args;
                    f.name = name~"."~f.origname;
                    f.isMethod = true;
                    _thisFs ~= f;

                    if(isImported) {
                        f.isExtern = true;
                    }

                    if(cast(immutable)[name,f.origname].into(MethodTable)) {
                        // Maybe overload?
                        if(typesToString(MethodTable[cast(immutable)[name,f.origname]].args) != typesToString(f.args)) {
                            // Overload
                            MethodTable[cast(immutable)[name,f.origname~typesToString(f.args)]] = f;
                        }
                        else Generator.error(loc," method '"~f.origname~"' has already been declared on "~to!string(MethodTable[cast(immutable)[name,f.origname]].loc)~" line!");
                    }
                    else MethodTable[cast(immutable)[name,f.origname]] = f;

                    methods ~= f;
                }
                else if(f.origname == "~this") {
                    _destructor = f;
                    _destructor.name = "~"~origname;
                    _destructor.type = f.type;
                    _destructor.isTemplatePart = isLinkOnce;
                    _destructor.namespacesNames = namespacesNames.dup;
                    _destructor.isComdat = isComdat;

                    Type outType = _this[0].type;
                    if(outType.instanceof!TypeStruct) outType = new TypePointer(outType);

                    if(_destructor.args.length>0 && _destructor.args[0].name == "this") _destructor.args = _destructor.args[1..$];
                    _destructor.args = [FuncArgSet("this",outType)];

                    if(isImported) {
                        _destructor.isExtern = true;
                        _destructor.check();
                        continue;
                    }

                    if(!_this[0].type.instanceof!TypeStruct) _destructor.block.nodes = _destructor.block.nodes ~ new NodeCall(
                            _destructor.loc,
                            new NodeIden(NeededFunctions["free"],_destructor.loc),
                            [new NodeIden("this",_destructor.loc)]
                    );

                    _destructor.check();
                }
                else if(f.origname.indexOf('(') != -1) {
                    f.isMethod = true;
                    f.isTemplatePart = isLinkOnce;
                    f.isComdat = isComdat;
                    if(isImported) f.isExtern = true;

                    TokType oper;
                    switch(f.origname[$-3..$]) {
                        case "(+)":
                            oper = TokType.Plus; f.name = name~"(+)";
                            break;
                        case "(=)":
                            oper = TokType.Equ; f.name = name~"(=)";
                            break;
                        case "==)":
                            oper = TokType.Equal; f.name = name~"(==)";
                            break;
                        case "!=)":
                            oper = TokType.Nequal; f.name = name~"(!=)";
                            break;
                        case "[])":
                            oper = TokType.Rbra; f.name = name~"([])";
                            break;
                        case "]=)":
                            oper = TokType.Lbra; f.name = name~"([]=)";
                            break;
                        default: assert(0);
                    }

                    f.name = f.name ~ typesToString(f.args);

                    if(f.origname[$-3..$] != "[])") operators[oper][typesToString(f.args)] = f;
                    else operators[oper][""] = f;
                    methods ~= f;
                }
                else if(f.origname == "~with") {
                    f.isMethod = true;
                    f.isTemplatePart = isLinkOnce;

                    f.name = "~with."~name;

                    Type outType = _this[0].type;
                    if(outType.instanceof!TypeStruct) outType = new TypePointer(outType);

                    f.args = [FuncArgSet("this",outType)];

                    _with = f;

                    if(isImported) {
                        _with.isExtern = true;
                        _with.check();
                        continue;
                    }
                }
                else {
                    Type outType = new TypePointer(new TypeStruct(name));
                    f.isTemplatePart = isLinkOnce;
                    if(_this !is null) {
                        outType = _this[0].type;
                        if(outType.instanceof!TypeStruct) outType = new TypePointer(outType);
                    }
                    if((f.args.length > 0 && f.args[0].name != "this") || f.args.length == 0) f.args = FuncArgSet("this",outType)~f.args;
                    f.name = name~"."~f.origname;
                    f.isMethod = true;

                    if(isImported) {
                        f.isExtern = true;
                    }

                    if(cast(immutable)[name,f.origname].into(MethodTable)) {
                        // Maybe overload?
                        if(typesToString(MethodTable[cast(immutable)[name,f.origname]].args) != typesToString(f.args)) {
                            // Overload
                            MethodTable[cast(immutable)[name,f.origname~typesToString(f.args)]] = f;
                        }
                        else Generator.error(loc," method '"~f.origname~"' has already been declared on "~to!string(MethodTable[cast(immutable)[name,f.origname]].loc)~" line!");
                    }
                    else MethodTable[cast(immutable)[name,f.origname]] = f;

                    methods ~= f;
                }
            }
        }
        return values.dup;
    }
    bool isTemplated = false;

    override LLVMValueRef generate() {
        if(templateNames.length > 0 || noCompile) return null;

        for(int i=0; i<mods.length; i++) {
            while(mods[i].name.into(AliasTable)) {
                if(NodeArray arr = AliasTable[mods[i].name].instanceof!NodeArray) {
                    mods[i].name = arr.values[0].instanceof!NodeString.value;
                    mods[i].value = arr.values[1].instanceof!NodeString.value;
                }
                else mods[i].name = AliasTable[mods[i].name].instanceof!NodeString.value;
            }
            switch(mods[i].name) {
                case "packed": isPacked = true; break;
                default: break;
            }
        }

        Generator.Structs[name] = LLVMStructCreateNamed(Generator.Context,toStringz(name));
        LLVMTypeRef[] params = getParameters(isTemplated);
        LLVMStructSetBody(Generator.Structs[name],params.ptr,cast(uint)params.length,isPacked);

        NodeFunc[] _listOfMethods;
        for(int i=0; i<elements.length; i++) {
            if(elements[i].instanceof!NodeFunc) _listOfMethods ~= elements[i].instanceof!NodeFunc;
        }

        if(_destructor is null) {
            _destructor = new NodeFunc("~"~origname,[],new NodeBlock([new NodeCall(loc,new NodeIden("std::free",loc),[new NodeIden("this",loc)])]),false,[],loc,new TypeVoid(),[]);
            _destructor.namespacesNames = namespacesNames.dup;
            _destructor.isComdat = isComdat;

            if(_this.length > 0 && _this[0].type.instanceof!TypePointer) {

                if(isImported) {
                    _destructor.isExtern = true;
                    _destructor.check();
                }
                else {
                    Type outType;
                    outType = _this[0].type;
                    if(outType.instanceof!TypeStruct) outType = new TypePointer(outType);

                    _destructor.args = [FuncArgSet("this",outType)];

                    _destructor.check();
                    _destructor.generate();
                }
            }
        }

        for(int i=0; i<_listOfMethods.length; i++) {
            if(_listOfMethods[i].origname != "this" && _listOfMethods[i].origname != "~this" && _listOfMethods[i].origname != "~with") _listOfMethods[i].check();
            _listOfMethods[i].generate();
        }

        return null;
    }

    Node[] copyElements() {
        Node[] _nodes;

        for(int i=0; i<elements.length; i++) {
            if(NodeFunc nf = elements[i].instanceof!NodeFunc) {
                _nodes ~= new NodeFunc(nf.name,nf.args.dup,new NodeBlock(nf.block.nodes.dup),nf.isExtern,nf.mods.dup,nf.loc,nf.type.copy(),nf.templateNames.dup);
            }
            else if(NodeVar nv = elements[i].instanceof!NodeVar) {
                _nodes ~= new NodeVar(nv.name,nv.value,nv.isExtern,nv.isConst,nv.isGlobal,nv.mods.dup,nv.loc,nv.t.copy(),nv.isVolatile);
            }
        }

        return _nodes;
    }

    LLVMTypeRef generateWithTemplate(string typesS, Type[] _types) {
        // Save all global-variables
        auto activeLoops = Generator.ActiveLoops.dup;
        auto builder = Generator.Builder;
        auto currBB = Generator.currBB;
        auto _scope = currScope;
        auto toReplace = Generator.toReplace.dup;

        string _fn = "<";

        for(int i=0; i<_types.length; i++) {
            //writeln("Structure: ",name,", Type: ",templateNames[i],", replaced: ",_types[i]);
            if(TypeStruct _ts = _types[i].instanceof!TypeStruct) if(!_ts.name.into(StructTable) && !_ts.types.empty) {
                //writeln("\t??? Replacing structure = ",_ts.name);
                Generator.GenerateType(_ts,loc);
                //writeln("\t!!!");
            }
            //writeln("End.");
            //writeln("Set ",templateNames[i]," to ",_types[i].toString());
            Generator.toReplace[templateNames[i]] = _types[i];
            _fn ~= templateNames[i]~",";
        }

        Generator.toReplace[name~_fn[0..$-1]~">"] = new TypeStruct(name~typesS);

        NodeStruct _struct = new NodeStruct(name~typesS,copyElements().dup,loc,"",[],mods.dup);
        _struct.isTemplated = true;
        _struct.isComdat = true;

        _struct.check();
        _struct.generate();

        Generator.ActiveLoops = activeLoops;
        Generator.Builder = builder;
        Generator.currBB = currBB;
        currScope = _scope;
        Generator.toReplace = toReplace.dup;

        return Generator.Structs[_struct.name];
    }
}

class NodeCast : Node {
    Type type;
    Node val;
    int loc;

    this(Type type, Node val, int loc) {
        this.type = type;
        this.val = val;
        this.loc = loc;
    }

    override Type getType() {return type;}
    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) val.check();
    }
    override LLVMValueRef generate() {
        LLVMValueRef gval;

        if(TypeBasic b = type.instanceof!TypeBasic) {
            if(b.type == BasicType.Bool && b.toString() != "bool") {
                return new NodeCast(new TypeStruct(b.toString()),val,loc).generate();
            }
            gval = val.generate();
            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMIntegerTypeKind) {
                if(b.isFloat()) {
                    return LLVMBuildSIToFP(
                        Generator.Builder,
                        gval,
                        Generator.GenerateType(type,loc),
                        toStringz("ifcast")
                    );
                }
                return LLVMBuildIntCast(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(type,loc),
                    toStringz("iicast")
                );
            }

            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMPointerTypeKind) {
                if(!b.isFloat()) return LLVMBuildPtrToInt(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(b,loc),
                    toStringz("picast")
                );
                return LLVMBuildLoad(Generator.Builder,LLVMBuildPointerCast(Generator.Builder,gval,LLVMPointerType(Generator.GenerateType(type,loc),0),toStringz("ptrc_")),toStringz("load_"));
            }

            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMStructTypeKind) {
                LLVMValueRef _temp = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(gval),toStringz("temp_"));
                LLVMBuildStore(Generator.Builder,gval,_temp);
                if(b.isFloat()) return LLVMBuildSIToFP(
                    Generator.Builder,
                    LLVMBuildPtrToInt(Generator.Builder,_temp,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi")),
                    Generator.GenerateType(type,loc),
                    toStringz("sfcast")
                );
                return LLVMBuildPtrToInt(Generator.Builder,_temp,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi"));
            }

            if(b.isFloat()) return LLVMBuildFPCast(
                Generator.Builder,
                gval,
                Generator.GenerateType(type,loc),
                toStringz("ffcast")
            );
            return LLVMBuildFPToSI(
                Generator.Builder,
                gval,
                Generator.GenerateType(type,loc),
                toStringz("ficast")
            );
        }
        gval = val.generate();
        if(TypePointer p = type.instanceof!TypePointer) {
            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMIntegerTypeKind) {
                return LLVMBuildIntToPtr(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(p,loc),
                    toStringz("ipcast")
                );
            }
            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMStructTypeKind) {
                if(NodeIden id = val.instanceof!NodeIden) {id.isMustBePtr = true; gval = id.generate();}
                else if(NodeGet ng = val.instanceof!NodeGet) {ng.isMustBePtr = true; gval = ng.generate();}
                else if(NodeIndex ind = val.instanceof!NodeIndex) {ind.isMustBePtr = true; gval = ind.generate();}
            }
            return LLVMBuildPointerCast(
                Generator.Builder,
                gval,
                Generator.GenerateType(type,loc),
                toStringz("ppcast")
            );
        }
        if(TypeFunc f = type.instanceof!TypeFunc) {
            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMIntegerTypeKind) {
                return LLVMBuildIntToPtr(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(f,loc),
                    toStringz("ipcast")
                );
            }
            return LLVMBuildPointerCast(
                Generator.Builder,
                gval,
                Generator.GenerateType(f,loc),
                toStringz("ppcast")
            );
        }
        if(TypeStruct s = type.instanceof!TypeStruct) {
            if(s.name.into(Generator.toReplace)) return new NodeCast(Generator.toReplace[s.name],val,loc).generate();
            LLVMValueRef ptrGval = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(gval),toStringz("temp_"));
            LLVMBuildStore(Generator.Builder,gval,ptrGval);
            return LLVMBuildLoad(Generator.Builder,LLVMBuildBitCast(Generator.Builder,ptrGval,LLVMPointerType(Generator.GenerateType(s,loc),0),toStringz("tempf_")),toStringz("load_"));
        }
        if(TypeMacroArg ma = type.instanceof!TypeMacroArg) {
            NodeType nt = currScope.macroArgs[ma.num].instanceof!NodeType;
            this.type = nt.ty;
            return this.generate();
        }
        if(TypeBuiltin b = type.instanceof!TypeBuiltin) {
            NodeBuiltin nb = new NodeBuiltin(b.name,b.args.dup,loc,b.block);
            nb.generate();

            this.type = nb.ty;
            return this.generate();
        }
        assert(0);
    }
}

class NodeMacro : Node {
    NodeBlock block;
    string name;
    string[] namespacesNames;
    string[] args;
    NodeRet ret;
    int loc;
    string origname;

    this(NodeBlock block, string name, string[] args, int loc) {
        this.block = block;
        this.name = name;
        this.args = args;
        this.loc = loc;
        this.origname = name;
    }

    override Type getType() {
        return ret.getType();
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) {
            if(namespacesNames.length>0) {
                name = namespacesNamesToString(namespacesNames,name);
            }
            Node[] nodes;
            for(int i=0; i<block.nodes.length; i++) {
                if(NodeRet r = block.nodes[i].instanceof!NodeRet) {
                    ret = r;
                }
                else {block.nodes[i].check(); nodes ~= block.nodes[i];}
            }
            this.block = new NodeBlock(nodes.dup);
            MacroTable[name] = this;
        }
    }

    override LLVMValueRef generate() {
        return null;
    }

    bool hasRet() {
        foreach(node; block.nodes) {
            if(node.instanceof!NodeRet) return true;
        }
        return false;
    }
}

class NodeUsing : Node {
    string namespace;
    int loc;

    this(string namespace, int loc) {
        this.namespace = namespace;
        this.loc = loc;
    }

    override Type getType() {
        return new TypeVoid();
    }

    override void check() {
        this.isChecked = true;
    }

    override LLVMValueRef generate() {
        foreach(var; VarTable) {
            if(var.namespacesNames.length>0) {
                string[] newNamespacesNames;
                for(int i=0; i<var.namespacesNames.length; i++) {
                    if(var.namespacesNames[i] != namespace) newNamespacesNames ~= var.namespacesNames[i];
                }
                var.namespacesNames = newNamespacesNames.dup;
                string oldname = var.name;
                var.name = namespacesNamesToString(var.namespacesNames, var.origname);
                VarTable[var.name] = var;
                //VarTable.remove(oldname);
                if(oldname.into(Generator.Globals)) Generator.Globals[var.name] = Generator.Globals[oldname];
                else if(oldname.into(AliasTable)) {
                    AliasTable[var.name] = AliasTable[oldname];
                }
                //Generator.Globals.remove(oldname);
            }
        }
        foreach(t; byKey(aliasTypes)) {
            if(t.indexOf("::") != -1) {
                string[] splitted = t.split("::");
                string[] newNamespaceNames;
                for(int i=0; i<splitted.length-1; i++) {
                    if(splitted[i] != namespace) newNamespaceNames ~= splitted[i];
                }
                aliasTypes[namespacesNamesToString(newNamespaceNames, splitted[splitted.length-1])] = aliasTypes[t].copy();
            }
        }
        foreach(f; FuncTable) {
            if(f.namespacesNames.length>0) {
                string[] newNamespacesNames;
                for(int i=0; i<f.namespacesNames.length; i++) {
                    if(f.namespacesNames[i] != namespace) newNamespacesNames ~= f.namespacesNames[i];
                }
                f.namespacesNames = newNamespacesNames.dup;
                string oldname = f.name;
                if(f.origname == "this") f.name = namespacesNamesToString(f.namespacesNames, f.name);
                else if(indexOf(f.origname,'~') != -1) {
                    f.name = namespacesNamesToString(f.namespacesNames,f.origname);
                }
                else f.name = namespacesNamesToString(f.namespacesNames, f.origname);
                if(oldname[$-1] == ']') f.name ~= typesToString(f.args);
                FuncTable[f.name] = f;
                if(into(oldname,Generator.Functions)) Generator.Functions[f.name] = Generator.Functions[oldname];
            }
        }
        foreach(var; MacroTable) {
            if(var.namespacesNames.length>0) {
                string[] newNamespacesNames;
                for(int i=0; i<var.namespacesNames.length; i++) {
                    if(var.namespacesNames[i] != namespace) newNamespacesNames ~= var.namespacesNames[i];
                }
                var.namespacesNames = newNamespacesNames.dup;
                var.name = namespacesNamesToString(var.namespacesNames, var.origname);
                MacroTable[var.name] = var;
            }
        }
        foreach(varr; StructTable) {
            NodeStruct var = new NodeStruct(varr.origname, varr.elements.dup, varr.loc, varr._extends, varr.templateNames.dup, varr.mods.dup);
            var._oldElements = varr._oldElements.dup;
            var.operators = varr.operators.dup;
            var.predefines = varr.predefines.dup;
            var.isImported = varr.isImported;
            var.isTemplated = varr.isTemplated;
            if(varr.namespacesNames.length>0) {
                for(int i=0; i<varr.namespacesNames.length; i++) {
                    if(varr.namespacesNames[i] != namespace) var.namespacesNames ~= varr.namespacesNames[i];
                }
                string oldname = varr.name;
                var.name = namespacesNamesToString(var.namespacesNames, var.origname);
                StructTable[var.name] = var;
                if(varr.name.into(Generator.Structs)) Generator.Structs[var.name] = Generator.Structs[oldname];
                if(oldname.into(FuncTable)) {
                    FuncTable[var.name] = FuncTable[oldname];
                    Generator.Functions[var.name] = Generator.Functions[oldname];
                }
                foreach(s; StructTable[oldname].elements) {
                    if(NodeFunc f = s.instanceof!NodeFunc) {
                        if(f.origname.indexOf("(") != -1) {
                            // Operator overload
                            // Don't need a rename
                        }
                        else if(f.origname != "this" && f.origname != "~this" && f.origname != "~with") {
                            string withArgs = f.origname~typesToString(f.args);
                            if(cast(immutable)[oldname,withArgs].into(MethodTable)) {
                                for(int i=0; i<f.args.length; i++) {
                                    if(TypeStruct ts = f.args[i].type.instanceof!TypeStruct) {
                                        if(ts.name == oldname) ts.name = var.name;
                                        f.args[i].type = ts;
                                    }
                                    else if(TypePointer tp = f.args[i].type.instanceof!TypePointer) {
                                        if(TypeStruct ts = tp.instance.instanceof!TypeStruct) {
                                            if(ts.name == oldname) ts.name = var.name;
                                            tp.instance = ts;
                                        }
                                        f.args[i].type = tp;
                                    }
                                }
                                MethodTable[cast(immutable)[var.name,f.origname~typesToString(f.args)]] = MethodTable[cast(immutable)[oldname,withArgs]];
                            }
                            else if(cast(immutable)[oldname,f.origname].into(MethodTable)) {
                                MethodTable[cast(immutable)[var.name,f.origname]] = MethodTable[cast(immutable)[oldname,f.origname]];
                            }
                        }
                        else {
                            if(var.name.into(FuncTable)) FuncTable[var.name] = FuncTable[oldname];
                        }
                    }
                }
                //StructTable.remove(oldname);

                foreach(k; byKey(structsNumbers)) {
                    if(k[0] == oldname) {
                        structsNumbers[cast(immutable)[var.name,k[1]]] = structsNumbers[k];
                        //structsNumbers.remove(k);
                    }
                }
            }
        }
        return null;
    }
}

class NodeNull : Node {
    Type maintype;
    LLVMTypeRef llvmtype;

    this() {}

    override void check() {
        this.isChecked = true;
    }

    override Type getType() {return maintype;}
    override LLVMValueRef generate() {
        if(maintype !is null) return LLVMConstNull(Generator.GenerateType(maintype,-5));
        if(llvmtype !is null) return LLVMConstNull(llvmtype);
        return LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(Generator.Context),0));
    }
}

class MacroGetArg : Node {
    int number;
    int loc;

    this(int number, int loc) {
        this.number = number;
        this.loc = loc;
    }

    override LLVMValueRef generate() {
        return currScope.macroArgs[number].generate();
    }
}

class NodeTypeof : Node {
    Node expr;
    int loc;
    Type t;

    this(Node expr, int loc) {
        this.expr = expr;
        this.loc = loc;
    }

    override Type getType() {
        return expr.getType();
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) expr.check();
    }
    override LLVMValueRef generate() {
        if(NodeInt i = expr.instanceof!NodeInt) {
            TypeBasic b;
            b.type = i.ty;
            t = b;
        }
        else if(expr.instanceof!NodeString) {
            t = new TypePointer(new TypeBasic("char"));
        }
        return null;
    }
}

class NodeItop : Node {
    Node I;
    Type t;
    int loc;

    this(Node I, Type t, int loc) {
        this.I = I;
        this.loc = loc;
        this.t = t;
    }

    override Type getType() {
        return t;
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) I.check();
    }
    override LLVMValueRef generate() {
        LLVMValueRef _int = I.generate();
        if(Generator.opts.runtimeChecks && !FuncTable[currScope.func].isNoChecks) {
            LLVMValueRef isNull = LLVMBuildICmp(
                Generator.Builder,
                LLVMIntNE,
                _int,
                LLVMConstInt(LLVMTypeOf(_int),0,false),
                toStringz("assert(number==0)_")
            );
            LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: an attempt to turn a number into a null pointer in itop!\n",false).generate()].ptr,2,toStringz(""));
        }
        return LLVMBuildIntToPtr(
            Generator.Builder,
            _int,
            Generator.GenerateType(t,loc),
            toStringz("itop")
        );
    }
}

class NodePtoi : Node {
    Node P;
    int loc;

    this(Node P, int loc) {
        this.P = P;
        this.loc = loc;
    }

    override Type getType() {
        return new TypeBasic("int");
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) P.check();
    }
    override LLVMValueRef generate() {
        LLVMValueRef ptr = P.generate();
        if(Generator.opts.runtimeChecks && !FuncTable[currScope.func].isNoChecks) {
            LLVMValueRef isNull = LLVMBuildICmp(
                Generator.Builder,
                LLVMIntNE,
                LLVMBuildPtrToInt(Generator.Builder,ptr,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                LLVMBuildPtrToInt(Generator.Builder,new NodeNull().generate(),LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                toStringz("assert(p==null)_")
            );
            LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: attempt to use a null pointer in ptoi!\n",false).generate()].ptr,2,toStringz(""));
        }
        return LLVMBuildPtrToInt(
            Generator.Builder,
            ptr,
            LLVMInt64TypeInContext(Generator.Context),
            toStringz("ptoi")
        );
    }
}

class NodeAddr : Node {
    Node expr;
    int loc;

    this(Node expr, int loc) {
        this.expr = expr;
        this.loc = loc;
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) expr.check();
    }
    override LLVMValueRef generate() {
        LLVMValueRef value;

        if(NodeGet g = expr.instanceof!NodeGet) {
            g.isMustBePtr = true;
            value = g.generate();
        }
        else {
            NodeIden id = expr.instanceof!NodeIden;
            value = currScope.getWithoutLoad(id.name);
        }

        if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind) {
            return LLVMBuildPtrToInt(
                Generator.Builder,
                value,
                LLVMInt32TypeInContext(Generator.Context),
                toStringz("itop")
            );
        }
        else if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMIntegerTypeKind) {
            return LLVMBuildIntCast(
                Generator.Builder,
                value,
                LLVMInt32TypeInContext(Generator.Context),
                toStringz("intcast")
            );
        }
        assert(0);
    }
}

class NodeType : Node {
    Type ty;
    int loc;

    this(Type ty, int loc) {
        this.ty = ty;
        this.loc = loc;
    }

    override Type getType() {return ty;}
    override void check() {
        this.isChecked = true;
    }
    override LLVMValueRef generate() {
        return new NodeSizeof(this,loc).generate();
    }
}

class NodeSizeof : Node {
    Node val;
    int loc;

    this(Node val, int loc) {
        this.val = val;
        this.loc = loc;
    }

    override Type getType() {return new TypeBasic("int");}

    override LLVMValueRef generate() {
        if(NodeType ty = val.instanceof!NodeType) {
            return LLVMBuildIntCast(Generator.Builder, LLVMSizeOf(Generator.GenerateType(ty.ty,loc)), LLVMInt32TypeInContext(Generator.Context), toStringz("cast"));
        }
        if(MacroGetArg mga = val.instanceof!MacroGetArg) {
            NodeSizeof newsizeof = new NodeSizeof(currScope.macroArgs[mga.number],loc);
            return newsizeof.generate();
        }
        return LLVMSizeOf(
            LLVMTypeOf(val.generate())
        );
    }
}

class NodeDebug : Node {
    string idenName;
    NodeBlock block;

    this(string idenName, NodeBlock block) {
        this.idenName = idenName;
        this.block = block;
    }

    override void check() {
        this.isChecked = true;
    }

    override LLVMValueRef generate() {
        if(idenName.into(AliasTable)) {
            block.check();
            return block.generate();
        }
        return null;
    }
}

class NodeLambda : Node {
    int loc;

    NodeRet[] rets;
    RetGenStmt[] genRets;
    LLVMBasicBlockRef exitBlock;
    LLVMBuilderRef builder;

    TypeFunc typ;
    Type type;
    NodeBlock block;

    string name;

    LLVMValueRef f;

    this(int loc, TypeFunc type, NodeBlock block, string name = "") {
        this.loc = loc;
        this.typ = type;
        this.block = block;
    }

    override Type getType() {
        return typ.main;
    }

    LLVMTypeRef[] generateTypes() {
        LLVMTypeRef[] ts;
        for(int i=0; i<typ.args.length; i++) {
            ts ~= Generator.GenerateType(typ.args[i],loc);
        }
        return ts.dup;
    }

    override LLVMValueRef generate() {
        type = typ.main;
        Generator.countOfLambdas += 1;

        Scope oldScope = currScope;
        LLVMBasicBlockRef oldBB = Generator.currBB;
        Loop[int] activeLoops = Generator.ActiveLoops.dup;
        LLVMBuilderRef oldBuilder = Generator.Builder;

        TypeFuncArg[] _args = typ.args.dup;
        FuncArgSet[] _fargs;
        for(int i=0; i<_args.length; i++) {
            _fargs ~= FuncArgSet(_args[i].name,_args[i].type);
        }

        LambdaTable["lambda"~to!string(Generator.countOfLambdas)] = this;
        NodeFunc nf = new NodeFunc("__RAVE_LAMBDA"~to!string(Generator.countOfLambdas),_fargs,block,false,[],loc,typ.main,[]);
        nf.check();
        nf.isComdat = true;
        LLVMValueRef func = nf.generate();

        currScope = oldScope;
        Generator.currBB = oldBB;
        Generator.ActiveLoops = activeLoops.dup;
        Generator.Builder = oldBuilder;
        return func;
    }
}

class NodeArray : Node {
    int loc;
    Node[] values;
    private LLVMTypeRef _type;
    bool isConst = true;

    this(int loc, Node[] values) {
        this.loc = loc;
        this.values = values.dup;
    }

    override Type getType() {
        return values[0].getType();
    }

    private LLVMValueRef[] getValues() {
        LLVMValueRef[] buff;
        for(int i=0; i<values.length; i++) {
            if(i != 0) buff ~= values[i].generate();
            else {
                LLVMValueRef v = values[i].generate();
                buff ~= v;
                this._type = LLVMTypeOf(v);
            }
            if(!LLVMIsConstant(buff[buff.length-1])) isConst = false;
        }
        return buff.dup;
    }

    override LLVMValueRef generate() {
        LLVMValueRef[] values = getValues();
        if(isConst) return LLVMConstArray(_type,values.ptr,cast(uint)values.length);
        LLVMValueRef arr = LLVMBuildAlloca(Generator.Builder,LLVMArrayType(_type,cast(uint)values.length),toStringz("temp_"));
        for(int i=0; i<values.length; i++) {
            LLVMBuildStore(Generator.Builder, values[i], Generator.byIndex(arr,[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),cast(ulong)i,false)]));
        }
        return LLVMBuildLoad(Generator.Builder,arr,toStringz("load_"));
    }
}

class NodeImport : Node {
    string[] files;
    string[] functions;
    int loc;

    this(string[] files, int loc, string[] functions) {
        this.files = files;
        this.loc = loc;
        this.functions = functions;
    }

    override LLVMValueRef generate() {
        Node[] nodes;
        Node[] _nodes;

        for(int i=0; i<files.length; i++) {
            if(canFind(_importedFiles,files[i]) || files[i] == Generator.file) continue;

            if(files[i].indexOf("/.rave") == files[i].length-6) {
                // Import all files from directory
                NodeImport imp = new NodeImport([],loc,functions.dup);
                foreach(string n; dirEntries(files[i][0..$-5], SpanMode.depth)) {
                    if(n.lastIndexOf(".rave") == n.length-5) imp.files ~= n;
                }
                imp.generate();
                continue;
            }

            if(!files[i].into(_parsed)) {
                string content = "alias __RAVE_IMPORTED_FROM = \""~Generator.file~"\"; "~readText(files[i]);
                Lexer l = new Lexer(content,1);
                Parser p = new Parser(l.getTokens(),1,files[i]);
                p.parseAll();
                nodes = p.getNodes().dup;
                _parsed[files[i]] = nodes.dup;
            }
            else {
                _nodes = _parsed[files[i]].dup;
                nodes = [];
                if(NodeVar v = _nodes[0].instanceof!NodeVar) {
                    if(v.name == "__RAVE_IMPORTED_FROM") v.value = new NodeString(Generator.file, false);
                }
                for(int j=0; j<_nodes.length; j++) {
                    nodes ~= _nodes[j].copy();
                }
            }

            for(int j=0; j<nodes.length; j++) {
                if(NodeFunc nf = nodes[j].instanceof!NodeFunc) {
                    if(!nf.isPrivate) nf.check();
                }
                else if(NodeNamespace nn = nodes[j].instanceof!NodeNamespace) {
                    nn.hidePrivated = true;
                    nn.check();
                }
                else nodes[j].check();
            }

            string oldFile = Generator.file;
            Generator.file = files[i];
            for(int j=0; j<nodes.length; j++) {
                if(NodeNamespace nn = nodes[j].instanceof!NodeNamespace) {
                    nn.isImport = true;
                }
                else if(NodeFunc nf = nodes[j].instanceof!NodeFunc) {
                    if(nf.isPrivate) continue;
                    nf.isExtern = true;
                }
                else if(NodeVar nv = nodes[j].instanceof!NodeVar) {
                    if(nv.isPrivate) continue;
                    nv.isExtern = true;
                }
                else if(NodeStruct ns = nodes[j].instanceof!NodeStruct) {
                    ns.isImported = true;
                }
                else if(NodeBuiltin nb = nodes[j].instanceof!NodeBuiltin) {
                    nb.isImport = true;
                }
                nodes[j].generate();
            }
            Generator.file = oldFile;
            _importedFiles ~= files[i];
        }
        return null;
    }
}

class NodeBuiltin : Node {
    string name;
    Node[] args;
    int loc;
    Type ty = null;
    NodeBlock block;
    bool isImport = false;
    bool isTopLevel = false;
    int CTId = 0;

    this(string name, Node[] args, int loc, NodeBlock block) {
        this.name = name;
        this.args = args.dup;
        this.loc = loc;
        this.block = block;
    }

    this(string name, Node[] args, int loc, NodeBlock block, Type ty, bool isImport, bool isTopLevel, int CTId) {
        this.name = name;
        this.args = args.dup;
        this.loc = loc;
        this.block = block;
        this.ty = ty;
        this.isImport = isImport;
        this.isTopLevel = isTopLevel;
        this.CTId = CTId;
    }

    override Node copy() {
        Node[] _args;
        for(int i=0; i<args.length; i++) _args ~= args[i].copy();

        Type _ty;
        if(ty !is null) _ty = ty.copy();
        else _ty = null;

        return new NodeBuiltin(name, _args, loc, block.copy().instanceof!NodeBlock, _ty, isImport, isTopLevel, CTId);
    }

    override Type getType() {
        switch(name) {
            case "trunc":
            case "fmodf":
                return args[0].getType();
            default: return new TypeVoid();
        }
    }

    override void check() {
        this.isChecked = true;
    }

    NodeType asType(int n, bool isCompTime = false) {
        if(NodeType nt = args[n].instanceof!NodeType) return nt;
        if(MacroGetArg mga = args[n].instanceof!MacroGetArg) {
            if(NodeIden ni = currScope.macroArgs[mga.number].instanceof!NodeIden) {
                return new NodeType(new TypeStruct(ni.name),loc);
            }
            return currScope.macroArgs[mga.number].instanceof!NodeType;
        }
        if(NodeIden id = args[n].instanceof!NodeIden) {
            if(id.name.into(Generator.toReplace)) {
                Type _t = Generator.toReplace[id.name];
                while(_t.toString().into(Generator.toReplace)) {
                    _t = Generator.toReplace[_t.toString()];
                }
                return new NodeType(_t,loc);
            }
            if(id.name == "void") return new NodeType(new TypeVoid(),loc);
            if(id.name != "bool" && id.name != "char" && id.name != "uchar"
            && id.name != "ushort" && id.name != "short" && id.name != "wchar"
            && id.name != "uwchar" && id.name != "uint" && id.name != "int"
            && id.name != "long" && id.name != "ulong" && id.name != "cent"
            && id.name != "ucent" && id.name != "float" && id.name != "double") {
                return new NodeType(new TypeStruct(id.name),loc);
            }
            return new NodeType(new TypeBasic(id.name),loc);
        }
        if(NodeBuiltin b = args[n].instanceof!NodeBuiltin) {
            Type _t;
            if(!isCompTime) {
                b.generate();
                _t = b.ty.copy();
            }
            else _t = b.comptime().instanceof!NodeType.ty.copy();
            return new NodeType(_t,loc);
        }
        assert(0);
    }

    string getStringFrom(string s) {
        if(currScope.has(s)) {
            LLVMValueRef v = currScope.getWithoutLoad(s,loc);
            string trueString = "";
            LLVMValueRef nextInst = LLVMGetNextInstruction(v);
            while(nextInst !is null) {
                string str = cast(string)fromStringz(LLVMPrintValueToString(nextInst));
                if(LLVMGetInstructionOpcode(nextInst) == LLVMStore) {
                       str = str.strip();
                        str = str[6..$];
                    if(str[0..3] == "i8*") {
                        if(str.indexOf("i8**") != -1 && str.indexOf("@_str") != -1 && str.indexOf(s) != -1) {
                            // We found the right value
                            str = str[str.indexOf("@_str")..$];
                            str = str[1..str.indexOf(",")];
                            str = cast(string)fromStringz(LLVMPrintValueToString(LLVMGetNamedGlobal(Generator.Module,toStringz(str))));
                            str = str[str.indexOf("constant")+"constant".length..$].strip();
                            str = str[str.indexOf("c")+2..str.lastIndexOf(",")-1];
                            trueString = str;
                        }
                    }
                }
                nextInst = LLVMGetNextInstruction(nextInst);
            }
            trueString = trueString[0..$-3];
            trueString = trueString.replace("\\0A","\n")
                .replace("\\09","\t")
                .replace("\\00","");
                return trueString;
        }
        assert(0);
    }

    string asStringIden(int n) {
        if(NodeIden id = args[n].instanceof!NodeIden) {
            string nam = id.name;
            while(nam.into(AliasTable)) {
                Node node = AliasTable[nam];
                if(NodeIden _id = node.instanceof!NodeIden) nam = _id.name;
                else if(NodeString str = node.instanceof!NodeString) nam = str.value;
                else {
                    writeln(node," ",name," ",args);
                    assert(0);
                }
            }
            return nam;
        }
        else if(NodeString str = args[n].instanceof!NodeString) return str.value;
        else if(NodeBool bo = args[n].instanceof!NodeBool) return to!string(bo.value);
        else if(NodeBuiltin nb = args[n].instanceof!NodeBuiltin) {
            Node nn = nb.comptime();
            if(NodeString nns = nn.instanceof!NodeString) return nns.value;
            if(NodeInt nni = nn.instanceof!NodeInt) return to!string(nni.value);
            assert(0);
        }
        assert(0);
    }

    BigInt asConstLong(int n) {
        BigInt result = 0;
        if(NodeIden id = args[n].instanceof!NodeIden) {
            string nam = id.name;
            while(nam.into(AliasTable)) {
                Node node = AliasTable[nam];
                if(NodeIden _id = node.instanceof!NodeIden) nam = _id.name;
                else if(NodeInt _int = node.instanceof!NodeInt) {
                    result = _int.value;
                    nam = "";
                }
            }
        }
        else if(NodeInt _int = args[n].instanceof!NodeInt) result = _int.value;
        return result;
    }

    string getAliasName(int n) {
        if(NodeIden id = args[n].instanceof!NodeIden) return id.name;
        else if(NodeString str = args[n].instanceof!NodeString) return str.value;
        assert(0);
    }

    override LLVMValueRef generate() {
        name = name.strip();
        switch(name) {
            case "baseType":
                Type prType = asType(0).ty;
                if(TypeArray ta = prType.instanceof!TypeArray) this.ty = ta.element;
                else if(TypePointer tp = prType.instanceof!TypePointer) this.ty = tp.instance;
                else this.ty = prType;
                return null;
            case "isNumeric":
                Type isNType = asType(0).ty;
                if(isNType.instanceof!TypeBasic) {
                    string s = isNType.toString();
                    if(s == "bool" || s == "char" || s == "int" || s == "short" || s == "long" || s == "cent" || s == "float" || s == "double") {
                        return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                    }
                }
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isFloat":
                Type isNType = asType(0).ty;
                if(isNType.toString() == "float" || isNType.toString() == "double") return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isPointer":
                Type isPType = asType(0).ty;
                if(isPType.instanceof!TypePointer) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isStructure":
                Type isSType = asType(0).ty;
                if(isSType.instanceof!TypeStruct) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "typesIsEquals":
                Type isEqType = asType(0).ty;
                Type isEqType2 = asType(1).ty;
                if(isEqType.toString() == isEqType2.toString()) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "typesIsNequals":
                Type isEqType = asType(0).ty;
                Type isEqType2 = asType(1).ty;
                if(isEqType.toString() != isEqType2.toString()) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "va_arg":
                return LLVMBuildVAArg(Generator.Builder,args[0].generate(),Generator.GenerateType(asType(1).ty,loc),toStringz("builtInVaArg"));
            case "addLibrary":
                for(int i=0; i<args.length; i++) {
                    if(!canFind(_libraries,args[i].instanceof!NodeString.value)) {
                        _libraries ~= args[i].instanceof!NodeString.value;
                    }
                }
                return null;
            case "random":
                BigInt minimum = asConstLong(0);
                BigInt maximum = asConstLong(1);
                BigInt _rand = uniform!"[]"(cast(long)minimum, cast(long)maximum);
                if(maximum >= int.max) {
                    if(maximum >= long.max) return LLVMConstIntOfString(LLVMInt128TypeInContext(Generator.Context), toStringz(_rand.toDecimalString()), 10);
                    return LLVMConstIntOfString(LLVMInt64TypeInContext(Generator.Context), toStringz(_rand.toDecimalString()), 10);
                }
                return LLVMConstIntOfString(LLVMInt64TypeInContext(Generator.Context), toStringz(_rand.toDecimalString()), 10);
            case "isConstType":
                if(asType(0).ty.instanceof!TypeConst) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "resetArgs":
                Generator.currentBuiltinArg = 0;
                return null;
            case "getArg":
                Type getArgType = asType(0).ty;
                int getArgNum = cast(int)args[1].instanceof!NodeInt.value;
                return new NodeCast(getArgType,new NodeIden("_RaveArg"~to!string(getArgNum),loc),loc).generate();
            case "getArgType":
                int getArgNum = cast(int)args[0].instanceof!NodeInt.value;
                ty = currScope.getVar("_RaveArg"~to!string(getArgNum),loc).t;
                if(TypeBasic tb = ty.instanceof!TypeBasic) {
                    if(ty.toString() != "bool" && tb.type == BasicType.Bool) ty = new TypeStruct(ty.toString());
                }
                return null;
            case "typeToString":
                Type typeToStr = asType(0).ty;
                return new NodeString(typeToStr.toString(),false).generate();
            case "skipArg":
                Generator.currentBuiltinArg += 1;
                return null;
            case "foreachArgs":
                for(int i=Generator.currentBuiltinArg; i<FuncTable[currScope.func].args.length; i++) {
                    block.generate();
                    Generator.currentBuiltinArg += 1;
                }
                return null;
            case "argsLength":
                return new NodeInt(FuncTable[currScope.func].args.length).generate();
            case "if":
                Node cond = args[0];
                if(NodeBinary nb = args[0].instanceof!NodeBinary) nb.isStatic = true;
                condStack[CTId] = cond;
                if(isImport) for(int i=0; i<block.nodes.length; i++) {
                    if(NodeBuiltin nb = block.nodes[i].instanceof!NodeBuiltin) nb.isImport = true;
                    else if(NodeNamespace nn = block.nodes[i].instanceof!NodeNamespace) nn.isImport = true;
                    else if(NodeFunc nf = block.nodes[i].instanceof!NodeFunc) nf.isExtern = true;
                    else if(NodeVar nv = block.nodes[i].instanceof!NodeVar) nv.isExtern = true;
                    else if(NodeStruct ns = block.nodes[i].instanceof!NodeStruct) ns.isImported = true;
                }
                NodeIf _if = new NodeIf(cond, block, null, loc, (currScope is null ? "" : currScope.func), true);
                _if.comptime();
                return null;
            case "else":
                Node cond = condStack[CTId];
                if(isImport) for(int i=0; i<block.nodes.length; i++) {
                    if(NodeBuiltin nb = block.nodes[i].instanceof!NodeBuiltin) nb.isImport = true;
                    else if(NodeNamespace nn = block.nodes[i].instanceof!NodeNamespace) nn.isImport = true;
                    else if(NodeFunc nf = block.nodes[i].instanceof!NodeFunc) nf.isExtern = true;
                    else if(NodeVar nv = block.nodes[i].instanceof!NodeVar) nv.isExtern = true;
                    else if(NodeStruct ns = block.nodes[i].instanceof!NodeStruct) ns.isImported = true;
                }
                NodeIf _if = new NodeIf(new NodeUnary(loc,TokType.Ne,cond),block,null,loc,(currScope is null ? "" : currScope.func),true);
                _if.comptime();
                return null;
            case "sizeOf":
                Type t = asType(0).ty;
                return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),cast(ulong)t.getSize(),false);
            case "callWithArgs":
                Node[] nodes;
                for(int i=Generator.currentBuiltinArg; i<FuncTable[currScope.func].args.length; i++) {
                    nodes ~= new NodeIden("_RaveArg"~to!string(i),loc);
                }
                for(int i=1; i<args.length; i++) nodes ~= args[i];
                return new NodeCall(loc,args[0],nodes.dup).generate();
            case "aliasExists":
                string s = getAliasName(0);
                if(s.into(AliasTable)) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "setCustomFree":
                NeededFunctions["free"] = asStringIden(0);
                return null;
            case "setCustomMalloc":
                NeededFunctions["malloc"] = asStringIden(0);
                return null;
            case "lengthOfCString":
                if(NodeString str = args[0].instanceof!NodeString) {
                    return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),str.value.length,false);
                }
                else {
                    string s = asStringIden(0);
                    return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),s.length,false);
                }
            case "execute":
                string s = asStringIden(0);
                if(currScope !is null && currScope.has(s)) s = getStringFrom(s);
                if(currScope !is null) s = "{"~s~"}";

                Lexer l = new Lexer(s,1);
                Parser p = new Parser(l.getTokens(),1,"(execute)");

                if(currScope is null) p.parseAll();
                else {
                    NodeBlock bl = p.parseBlock(currScope.func).instanceof!NodeBlock;
                    bl.check();
                    bl.generate();
                    return null;
                }

                Node[] nodes = p.getNodes().dup;
                for(int i=0; i<nodes.length; i++) nodes[i].check();
                for(int i=0; i<nodes.length; i++) nodes[i].generate();
                return null;
            case "setRuntimeChecks":
                string s = asStringIden(0);
                Generator.opts.runtimeChecks = to!bool(s);
                return null;
            case "error":
                string allString = "";
                for(int i=0; i<args.length; i++) {
                    allString ~= asStringIden(i);
                }
                Generator.error(loc,allString);
                return null;
            case "warning":
                if(Generator.opts.disableWarnings) return null;
                string allString = "";
                for(int i=0; i<args.length; i++) {
                    allString ~= asStringIden(i);
                }
                if(!canFind(warningStack,allString)) {
                    Generator.warning(loc,allString);
                    warningStack ~= allString;
                }
                return null;
            case "echo":
                string allString = "";
                for(int i=0; i<args.length; i++) {
                    allString ~= asStringIden(i);
                }
                writeln(allString);
                return null;
            case "getCurrArg":
                Type getArgType = asType(0).ty;
                int getArgNum = Generator.currentBuiltinArg;
                return new NodeCast(getArgType,new NodeIden("_RaveArg"~to!string(getArgNum),loc),loc).generate();
            case "getCurrArgType":
                int getArgNum = Generator.currentBuiltinArg;
                this.ty = currScope.getVar("_RaveArg"~to!string(getArgNum),loc).t;
                return null;
            case "getStructElementType":
                TypeStruct ts = asType(0).ty.instanceof!TypeStruct;
                string n = asStringIden(1);
                ty = structsNumbers[cast(immutable)[ts.name,n]].var.t;
                return null;
            case "replaceIntTypes":
                Type one = asType(0).ty;
                Type two = asType(1).ty;
                BasicType onebt = one.instanceof!TypeBasic.type;
                BasicType twobt = two.instanceof!TypeBasic.type;
                Generator.changeableTypes[onebt] = Generator.changeableTypes[twobt];
                return null;
            case "compileAndLink":
                string s = asStringIden(0);
                if(s[0] == '<') {
                    string exePath = thisExePath();
                    if(exePath[$-3..$] == "exe") exePath = exePath[$-4..$];
                    _addToImport ~= exePath[0..$-4]~s[1..$-1]~".rave";
                }
                else _addToImport ~= dirName(ASTMainFile)~"/"~s[1..$-1]~".rave";
                return null;
            case "trunc":
                LLVMValueRef val = args[0].generate();
                if(LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMFloatTypeKind && LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMDoubleTypeKind) {
                    return val;
                }
                return LLVMBuildSIToFP(Generator.Builder,
                    LLVMBuildFPToSI(
                        Generator.Builder,
                        val,
                        LLVMInt64TypeInContext(Generator.Context),
                        toStringz("trunc1_")
                    ),
                    LLVMTypeOf(val),
                    toStringz("trunc2_")
                );
            case "fmodf":
                LLVMValueRef _x = args[0].generate();
                LLVMValueRef _y = args[1].generate();
                LLVMTypeRef _t = LLVMTypeOf(_x);
                LLVMValueRef _div;
                LLVMValueRef _result;
                if(LLVMGetTypeKind(_t) == LLVMFloatTypeKind || LLVMGetTypeKind(_t) == LLVMDoubleTypeKind) {
                    if(LLVMGetTypeKind(_t) != LLVMGetTypeKind(LLVMTypeOf(_y))) {
                        if(LLVMGetTypeKind(_t) == LLVMDoubleTypeKind) _y = LLVMBuildFPCast(
                            Generator.Builder, _y, _t, toStringz("fmodf_ftod")
                        );
                        else _x = LLVMBuildFPCast(
                            Generator.Builder, _x, LLVMTypeOf(_y), toStringz("fmodf_ftod")
                        );
                    }
                    _div = LLVMBuildFDiv(
                        Generator.Builder,
                        _x,
                        _y,
                        toStringz("fmodf_fdiv_")
                    ); 
                    _result = LLVMBuildSIToFP(Generator.Builder,
                        LLVMBuildFPToSI(
                            Generator.Builder,
                            _div,
                            LLVMInt64TypeInContext(Generator.Context),
                            toStringz("fmodf_trunc1_")
                        ),
                        LLVMTypeOf(_x),
                        toStringz("fmodf_trunc2_")
                    );
                    _result = LLVMBuildFMul(
                        Generator.Builder,
                        _result,
                        _y,
                        toStringz("fmodf_fmul_")
                    );
                    _result = LLVMBuildFSub(
                        Generator.Builder,
                        _x,
                        _result,
                        toStringz("fmodf_")
                    );
                }
                else {
                    _div = LLVMBuildSDiv(
                        Generator.Builder,
                        _x,
                        _y,
                        toStringz("fmodf_sdiv_")
                    );
                    _result = LLVMBuildMul(
                        Generator.Builder,
                        _div,
                        _y,
                        toStringz("fmodf_mul_")
                    );
                    _result = LLVMBuildSub(
                        Generator.Builder,
                        _x,
                        _result,
                        toStringz("fmodf_")
                    );
                }
                return _result;
            default: break;
        }
        Generator.error(loc,"Builtin with the name '"~name~"' does not exist");
        return null;
    }

    override Node comptime() {
        name = name.strip();
        switch(name) {
            case "random":
                BigInt minimum = asConstLong(0);
                BigInt maximum = asConstLong(1);
                return new NodeInt(uniform!"[]"(cast(long)minimum, cast(long)maximum));
            case "argsLength":
                return new NodeInt(FuncTable[currScope.func].args.length);
            case "typeToString":
                Type typeToStr = asType(0,true).ty;
                return new NodeString(typeToStr.toString(),false);
            case "typesIsEquals":
                Type one = asType(0).ty;
                Type two = asType(1).ty;
                return new NodeBool(one.toString() == two.toString());
            case "typesIsNequals":
                Type one = asType(0).ty;
                Type two = asType(1).ty;
                return new NodeBool(one.toString() != two.toString());
            case "sizeOf":
                Type t = asType(0).ty;
                return new NodeInt(cast(ulong)t.getSize());
            case "isNumeric":
                Type isNType = asType(0).ty;
                if(isNType.instanceof!TypeBasic) {
                    string s = isNType.toString();
                    if(s == "bool" || s == "char" || s == "int" || s == "short" || s == "long" || s == "cent" || s == "float" || s == "double" || s == "ucent" || s == "ulong" || s == "uint" || s == "ushort" || s == "uchar") {
                        return new NodeBool(true);
                    }
                }
                return new NodeBool(false);
            case "aliasExists":
                return new NodeBool(getAliasName(0).into(AliasTable));
            case "setCustomFree":
                NeededFunctions["free"] = asStringIden(0);
                break;
            case "setCustomMalloc":
                NeededFunctions["malloc"] = asStringIden(0);
                break;
            case "getCurrArgType":
                int getArgNum = Generator.currentBuiltinArg;
                this.ty = currScope.getVar("_RaveArg"~to!string(getArgNum),loc).t;
                return new NodeType(this.ty,loc);
            case "getStructElementType":
                TypeStruct ts = asType(0,true).ty.instanceof!TypeStruct;
                string n = asStringIden(1);
                if(ts !is null) return new NodeType(structsNumbers[cast(immutable)[ts.name,n]].var.t,loc);
                return new NodeType(new TypeVoid(),loc);
            case "getTemplateType":
                TypeStruct ts = asType(0,true).ty.instanceof!TypeStruct;
                if(ts !is null) return new NodeType(ts.types[args[1].instanceof!NodeInt.value.toLong()],loc);
                return new NodeType(new TypeVoid(),loc);
            case "getTemplateStructure":
                TypeStruct ts = asType(0,true).ty.instanceof!TypeStruct;
                if(ts !is null) return new NodeType(new TypeStruct(ts.name[0..ts.name.indexOf('<')]),loc);
                return new NodeType(new TypeVoid(),loc);
            case "contains":
                string str = asStringIden(0);
                string str2 = asStringIden(1);
                return new NodeBool(str.indexOf(str2) != -1);
            default: break;
        }
        return null;
    }
}

class NodeFor : Node {
    int loc;

    Node[] _Sets;
    NodeBinary _Condition;
    Node[] _Afters;
    NodeBlock block;
    string f;

    override Type getType() {
        return new TypeVoid();
    }

    this(Node[] _Sets, NodeBinary _Condition, Node[] _Afters, NodeBlock block, string f, int loc) {
        this._Sets = _Sets.dup;
        this._Condition = _Condition;
        this._Afters = _Afters.dup;
        this.f = f;
        this.loc = loc;
        this.block = block;
    }

    override LLVMValueRef generate() {
        auto oldLocalVars = currScope.localVars.dup;
        auto oldLocalScope = currScope.localscope.dup;

        for(int i=0; i<_Sets.length; i++) {
            _Sets[i].check();
            _Sets[i].generate();
        }

        block.nodes ~= _Afters;

        LLVMValueRef toret = new NodeWhile(_Condition,block,loc,f).generate();

        currScope.localscope = oldLocalScope;
        currScope.localVars = oldLocalVars;

        return toret;
    }
}

class NodeSwitch : Node {
    int loc;

    Node[] expressions;
    NodeBlock[] blocks;
} // TODO

class NodeWith : Node {
    int loc;
    Node expr;
    NodeBlock bl;

    this(int loc, Node expr, NodeBlock bl) {
        this.loc = loc;
        this.expr = expr;
        this.bl = bl;
    }

    override Node copy() {
        return new NodeWith(loc, expr.copy(), bl.copy().instanceof!NodeBlock);
    }

    override Type getType() {
        return new TypeVoid();
    }

    string getVarFromBottom() {
        Node curr = expr;

        while(!curr.instanceof!NodeIden) {
            if(NodeGet ng = curr.instanceof!NodeGet) curr = ng.base;
            else if(NodeIndex ind = curr.instanceof!NodeIndex) curr = ind.element;
            else if(NodeCall nc = curr.instanceof!NodeCall) curr = nc.func;
        }

        return curr.instanceof!NodeIden.name;
    }

    override LLVMValueRef generate() {
        expr.check();
        LLVMValueRef val;

        if(NodeIden id = expr.instanceof!NodeIden) {id.isMustBePtr = true; val = id.generate();}
        else if(NodeGet ng = expr.instanceof!NodeGet) {ng.isMustBePtr = true; val = ng.generate();}
        else if(NodeIndex ind = expr.instanceof!NodeIndex) {ind.isMustBePtr = true; val = ind.generate();}
        else val = expr.generate();

        bl.generate();
        string structName = "";

        if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMVoidTypeKind || LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind || LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMFloatTypeKind) val = currScope.getWithoutLoad(getVarFromBottom());

        if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val))) == LLVMPointerTypeKind) {
            val = LLVMBuildLoad(Generator.Builder,val,toStringz("load3612_"));
        }

        if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMStructTypeKind) structName = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(val)));
        else structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(val))));

        if(StructTable[structName]._with !is null) return LLVMBuildCall(
            Generator.Builder,
            Generator.Functions[StructTable[structName]._with.name],
            [val].ptr, 1,
            toStringz("")
        );

        return LLVMBuildCall(
            Generator.Builder,
            Generator.Functions[StructTable[structName]._destructor.name],
            [val].ptr, 1,
            toStringz("")
        );
    }
}

class NodeAsm : Node {
    string line;
    string additions = "";
    Node[] values;
    bool isVolatile = false;
    Type t;

    this(string line, bool isVolatile, Type t, string additions, Node[] values) {
        this.line = line;
        this.isVolatile = isVolatile;
        this.t = t;
        this.additions = additions;
        this.values = values.dup;
    }

    override Node copy() {
        Node[] _values;
        for(int i=0; i<values.length; i++) _values ~= values[i].copy();
        return new NodeAsm(line, isVolatile, (t is null ? null : t.copy()), additions, _values);
    }

    override Type getType() {
        return t;
    }

    override LLVMValueRef generate() {
        // asm volatile ("movl %%ebp, %0" : "=r" (stack_top));
        // stack_top = asm(long,"movl %%ebp, %0","=r");
        LLVMValueRef[] values;
        LLVMTypeRef[] ts;
        for(int i=0; i<this.values.length; i++) {
            LLVMValueRef val = this.values[i].generate();
            values ~= val;
            ts ~= LLVMTypeOf(val);
        }

        LLVMValueRef v = LLVMGetInlineAsm(
            LLVMFunctionType(Generator.GenerateType(t,-6),ts.ptr,cast(uint)ts.length,false),
            cast(char*)toStringz(line),
            line.length,
            cast(char*)toStringz(additions),
            additions.length,
            isVolatile,
            false,
            LLVMInlineAsmDialectATT
        );

        return LLVMBuildCall(Generator.Builder,v,values.ptr,cast(uint)values.length,toStringz(t.instanceof!TypeVoid ? "" : "v_"));
    }
}

class NodeMixin : Node {
    string name;
    string origname;
    int loc;
    NodeBlock block;
    string[] argumentsNames;
    string[] templateNames;
    string[] namespacesNames;

    this(string name, int loc, NodeBlock block, string[] templateNames, string[] argumentsNames) {
        this.loc = loc;
        this.block = block;
        this.templateNames = templateNames.dup;
        this.name = name;
        this.origname = name;
        this.argumentsNames = argumentsNames.dup;
    }

    override Node copy() {
        return new NodeMixin(origname, loc, block.copy().instanceof!NodeBlock, templateNames, argumentsNames);
    }

    override void check() {
        bool oldCheck = isChecked;
        isChecked = true;

        if(!oldCheck) {
            if(namespacesNames.length>0) {
                name = namespacesNamesToString(namespacesNames,name);
            }

            if(name.into(MixinTable)) {
                checkError(loc,"a mixin with '"~name~"' name already exists on "~to!string(MixinTable[name].loc)~" line!");
            }
            MixinTable[name] = this;
        }
    }

    override LLVMValueRef generate() {
        block.check();
        return block.generate();
    }
}

class NodeDone : Node {
    LLVMValueRef value;

    this(LLVMValueRef value) {
        this.value = value;
    }

    override Type getType() {
        return llvmTypeToType(LLVMTypeOf(value));
    }

    override LLVMValueRef generate() {
        return value;
    }
}

class NodeSlice : Node {
    int loc;
    Node start;
    Node end;
    Node value;
    bool isConst = true;
    bool isMustBePtr = false;
    bool dontLoad = false;

    this(int loc, Node start, Node end, Node value, bool isConst) {
        this.loc = loc;
        this.start = start;
        this.end = end;
        this.value = value;
        this.isConst = isConst;
    }

    override Node copy() {
        return new NodeSlice(loc, start.copy(), end.copy(), value.copy(), isConst);
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) {
            start.check();
            end.check();
            value.check();
        }
    }

    override Type getType() {
        Type _t = value.getType();

        if(TypePointer tp = _t.instanceof!TypePointer) return tp;
        if(TypeArray ta = _t.instanceof!TypeArray) return new TypePointer(ta.element);
        assert(0);
    }

    LLVMValueRef binSet(Node n) {
        LLVMValueRef val = n.generate();
        if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMConstantArrayValueKind) {
            LLVMValueRef v = LLVMBuildAlloca(Generator.Builder,LLVMTypeOf(val),toStringz("temp_"));
            LLVMBuildStore(Generator.Builder,val,v);
            val = v;
        }

        LLVMValueRef gValue;

        if(NodeIden id = value.instanceof!NodeIden) gValue = currScope.getWithoutLoad(id.name,loc);
        else if(NodeGet ng = value.instanceof!NodeGet) {
            ng.isMustBePtr = true;
            gValue = ng.generate();
        }
        else if(NodeIndex ind = value.instanceof!NodeIndex) {
            ind.isMustBePtr = true;
            gValue = ind.generate();
        }
        else gValue = value.generate();

        if(LLVMGetTypeKind(LLVMTypeOf(gValue)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(gValue))) == LLVMPointerTypeKind) gValue = LLVMBuildLoad(Generator.Builder, gValue, toStringz("load_"));

        if(isConst) {
            for(int i=cast(int)(start.comptime().instanceof!NodeInt.value); i<end.comptime().instanceof!NodeInt.value; i++) {
                LLVMBuildStore(
                    Generator.Builder,
                    LLVMBuildLoad(
                        Generator.Builder,
                        Generator.byIndex(val,[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),cast(ulong)i,false)]),
                        toStringz("load_")
                    ),
                    Generator.byIndex(gValue,[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),cast(ulong)i,false)])
                );
            }
        }
        else {
            NodeFor _f = new NodeFor(
                [new NodeVar("i",start,false,false,false,[],loc,new TypeBasic("int"))],
                new NodeBinary(TokType.Less, new NodeIden("i",loc), end, loc),
                [new NodeBinary(TokType.PluEqu, new NodeIden("i",loc), new NodeInt(1),loc)],
                new NodeBlock([
                    new NodeBinary(
                        TokType.Equ,
                        new NodeIndex(new NodeDone(gValue),[new NodeIden("i",loc)],loc),
                        new NodeIndex(new NodeDone(val), [new NodeIden("i",loc)],loc),
                        loc
                    )
                ]),
                currScope.func,
                loc
            );
            _f.check();
            _f.generate();
        }

        return gValue;
    }

    override LLVMValueRef generate() {
        LLVMValueRef gValue;

        if(NodeIden id = value.instanceof!NodeIden) gValue = currScope.getWithoutLoad(id.name,loc);
        else if(NodeGet ng = value.instanceof!NodeGet) {
            ng.isMustBePtr = true;
            gValue = ng.generate();
        }
        else if(NodeIndex ind = value.instanceof!NodeIndex) {
            ind.isMustBePtr = true;
            gValue = ind.generate();
        }
        else gValue = value.generate();

        if(LLVMGetTypeKind(LLVMTypeOf(gValue)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(gValue))) == LLVMPointerTypeKind) gValue = LLVMBuildLoad(Generator.Builder, gValue, toStringz("load_"));

        LLVMTypeRef _type;

        if(LLVMGetTypeKind(LLVMTypeOf(gValue)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(gValue))) == LLVMPointerTypeKind) {
            _type = LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(gValue)));
        }
        else _type = LLVMGetElementType(LLVMTypeOf(gValue));

        LLVMValueRef container;

        if(isConst) {
            Node _length = new NodeBinary(TokType.Minus,end,start,loc).comptime();
            container = LLVMBuildAlloca(Generator.Builder,LLVMArrayType(_type,cast(uint)_length.comptime().instanceof!NodeInt.value),toStringz("val_"));
        }
        else {
            LLVMValueRef cl = new NodeCall(loc,new NodeIden(NeededFunctions["malloc"],loc),[new NodeBinary(TokType.Minus,end,start,loc)]).generate();
            container = LLVMBuildPointerCast(Generator.Builder,cl,LLVMPointerType(_type,0),toStringz("ptc_"));
        }

        NodeFor _f = new NodeFor(
            [new NodeVar("i",start,false,false,false,[],loc,new TypeBasic("int")), new NodeVar("j",new NodeInt(0),false,false,false,[],loc,new TypeBasic("int"))],
            new NodeBinary(TokType.Less, new NodeIden("i",loc), end, loc),
            [new NodeBinary(TokType.PluEqu, new NodeIden("i",loc), new NodeInt(1),loc), new NodeBinary(TokType.PluEqu, new NodeIden("j",loc), new NodeInt(1),loc)],
            new NodeBlock([
                new NodeBinary(
                    TokType.Equ,
                    new NodeIndex(new NodeDone(container),[new NodeIden("j",loc)],loc),
                    new NodeIndex(new NodeDone(gValue), [new NodeIden("i",loc)],loc),
                    loc
                )
            ]),
            currScope.func,
            loc
        );
        _f.check();
        _f.generate();
        if(isConst) {
            if(!isMustBePtr) return LLVMBuildLoad(Generator.Builder,container,toStringz("load_"));
            return container;
        }
        else {
            if(!isMustBePtr) return container;
            else return Generator.byIndex(container,[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),0,false)]);
        }
    }
}

class NodeAliasType : Node {
    int loc;
    string name;
    string origname;
    Type value;
    string[] namespacesNames;

    this(int loc, string name, Type value) {
        this.loc = loc;
        this.name = name;
        this.origname = name;
        this.value = value;
    }

    override Node copy() {
        return new NodeAliasType(loc, origname, value.copy());
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;

        if(!oldCheck) {
            while(name.into(AliasTable)) {
                name = AliasTable[name].instanceof!NodeIden.name;
            }
            if(namespacesNames.length>0) {
                name = namespacesNamesToString(namespacesNames,name);
            }
            aliasTypes[name] = value;
        }
    }

    override LLVMValueRef generate() {
        return null;
    }

    override Node comptime() {
        return new NodeType(value,loc);
    }
}

class NodeTry : Node {
    int loc;
    NodeBlock block;

    this(int loc, NodeBlock block) {
        this.loc = loc;
        this.block = block;
    }

    override Node copy() {
        return new NodeTry(loc, block.copy().instanceof!NodeBlock);
    }

    override void check() {
        bool oldCheck = isChecked;
        this.isChecked = true;
        if(!oldCheck) block.check();
    }

    override LLVMValueRef generate() {
        currScope.inTry = true;
            block.generate();
        currScope.inTry = false;
        return null;
    }
}
