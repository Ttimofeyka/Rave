// Licensed by LGPL-3.0-or-later

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

Node[string] AliasTable;
NodeFunc[string] FuncTable;
NodeVar[string] VarTable;
NodeStruct[string] StructTable;
NodeMacro[string] MacroTable;
bool[string] ConstVars;
NodeFunc[string[]] MethodTable;
NodeLambda[string] LambdaTable;
string[] _libraries;

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

extern(C) LLVMBuilderRef LLVMCreateBuilderInContext(LLVMContextRef context);

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
    int optLevel;

    this(string file, int optLevel) {
        this.file = file;
        this.optLevel = optLevel;
        Context = LLVMContextCreate();
        Module = LLVMModuleCreateWithNameInContext(toStringz("rave"),Context);
        LLVMInitializeAllTargets();
        LLVMInitializeAllAsmParsers();
        LLVMInitializeAllAsmPrinters();
        LLVMInitializeAllTargetInfos();
        LLVMInitializeAllTargetMCs();
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

    void error(int loc,string msg) {
        pragma(inline,true);
        writeln("\033[0;31mError in '",file,"' file on "~to!string(loc)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
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

    LLVMTypeRef GenerateType(Type t, int loc = -1) {
        pragma(inline,true);
        if(t is null) return LLVMPointerType(
            LLVMInt8TypeInContext(Generator.Context),
            0
        );
        if(t.instanceof!TypeAlias) return null;
        if(t.instanceof!TypeBasic) {
            TypeBasic b = cast(TypeBasic)t;
            switch(b.type) {
                case BasicType.Bool: return LLVMInt1TypeInContext(Generator.Context);
                case BasicType.Char:
                case BasicType.Uchar:
                    return LLVMInt8TypeInContext(Generator.Context);
                case BasicType.Short: 
                case BasicType.Ushort:
                    return LLVMInt16TypeInContext(Generator.Context);
                case BasicType.Int: 
                case BasicType.Uint:
                    return LLVMInt32TypeInContext(Generator.Context);
                case BasicType.Long: 
                case BasicType.Ulong:
                    return LLVMInt64TypeInContext(Generator.Context);
                case BasicType.Cent: return LLVMInt128TypeInContext(Generator.Context);
                case BasicType.Float: return LLVMFloatTypeInContext(Generator.Context);
                case BasicType.Double: return LLVMDoubleTypeInContext(Generator.Context);
                default: assert(0);
            }
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
                Generator.error(loc,"Unknown structure '"~s.name~"'!");
            }
            return Generator.Structs[s.name];
        }
        if(t.instanceof!TypeVoid) return LLVMVoidTypeInContext(Generator.Context);
        if(TypeFunc f = t.instanceof!TypeFunc) {
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
                if(!ni.name.into(Generator.Structs)) Generator.error(loc,"Unknown structure '"~ni.name~"'!");
                return Generator.Structs[ni.name];
            }
        }
        if(TypeBuiltin tb = t.instanceof!TypeBuiltin) {
            NodeBuiltin nb = new NodeBuiltin(tb.name,tb.args.dup,loc); nb.generate();
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

    void addAttribute(string name, LLVMAttributeIndex index, LLVMValueRef ptr) {
        int id = LLVMGetEnumAttributeKindForName(toStringz(name),cast(int)(name.length));
        if(id == 0) {
            error(-1,"Unknown attribute '"~name~"'!");
        }
        LLVMAttributeRef attr = LLVMCreateEnumAttribute(Generator.Context,id,0);
        LLVMAddAttributeAtIndex(ptr,index,attr);
    }
}

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
        if(!n.into(args)) {
            writeln(loc);
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
        else return LLVMGetParam(
            LambdaTable[func].f,
            cast(uint)args[n]
        );
    }

    NodeVar getVar(string n, int line) {
        pragma(inline,true);
        if(n.into(localscope)) return localVars[n];
        if(n.into(Generator.Globals)) return VarTable[n];
        if(n.into(args)) return argVars[n];
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

class Node 
{
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

    Node comptime() {return null;}
}

class NodeNone : Node {}

class NodeInt : Node {
    ulong value;
    BasicType ty;
    Type _isVarVal = null;

    this(ulong value) {
        this.value = value;
    }

    override LLVMValueRef generate() {
        if(_isVarVal.instanceof!TypeBasic) {
            ty = _isVarVal.instanceof!TypeBasic.type;
            switch(ty) {
                case BasicType.Bool: return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),value,false);
                case BasicType.Char: return LLVMConstInt(LLVMInt8TypeInContext(Generator.Context),value,false);
                case BasicType.Short: return LLVMConstInt(LLVMInt16TypeInContext(Generator.Context),value,false);
                case BasicType.Int: return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),value,false);
                case BasicType.Long: return LLVMConstInt(LLVMInt64TypeInContext(Generator.Context),value,false);
                case BasicType.Cent: return LLVMConstInt(LLVMInt128TypeInContext(Generator.Context),value,false);
                default: break;
            }
        }
        if(value < int.max) {
            ty = BasicType.Int;
            return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),value,false);
        }
        if(value < long.max) {
            ty = BasicType.Long;
            return LLVMConstInt(LLVMInt64TypeInContext(Generator.Context),value,false);
        }
        ty = BasicType.Cent;
        return LLVMConstInt(LLVMInt128TypeInContext(Generator.Context), value, false);
        assert(0);
    }

    override void debugPrint() {
        writeln("NodeInt("~to!string(value)~")");
    }
}

class NodeFloat : Node {
    private float value;

    this(float value) {
        this.value = value;
    }

    override LLVMValueRef generate() {
        string val = to!string(value);
        string[] parts = val.split(".");
        if(parts.length==1) return LLVMConstReal(LLVMFloatTypeInContext(Generator.Context),value);
        if(parts.length>1 && parts[1].length<double.max_exp) return LLVMConstReal(LLVMFloatTypeInContext(Generator.Context),value);
        return LLVMConstReal(LLVMDoubleTypeInContext(Generator.Context),value);
    }

    override void debugPrint() {
        writeln("NodeFloat("~to!string(value)~")");
    }
}

class NodeString : Node {
    private string value;

    this(string value) {
        this.value = value;
    }

    override LLVMValueRef generate() {
        LLVMValueRef globalstr = LLVMAddGlobal(
            Generator.Module,
            LLVMArrayType(LLVMInt8TypeInContext(Generator.Context),cast(uint)value.length+1),
            toStringz("_str")
        );
        LLVMSetGlobalConstant(globalstr, true);
        LLVMSetUnnamedAddr(globalstr, true);
        LLVMSetLinkage(globalstr, LLVMPrivateLinkage);
        LLVMSetAlignment(globalstr, 1);
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

    this(char value) {
        this.value = value;
    }

    override LLVMValueRef generate() {
        return LLVMConstInt(LLVMInt8TypeInContext(Generator.Context),to!ulong(value),false);
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

    this(TokType operator, Node first, Node second, int line) {
        this.first = first;
        this.second = second;
        this.operator = operator;
        this.loc = line;
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
        else if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
            if(LLVMGetTypeKind(LLVMGetElementType(t)) == LLVMStructTypeKind) {
                string name = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(t)));
                return new TypeStruct((name.indexOf('<') == -1) ? name : name[0..name.indexOf('<')]);
            }
            return new TypePointer(null);
        }
        else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) {
            string name = cast(string)fromStringz(LLVMGetStructName(t));
            return new TypeStruct((name.indexOf('<') == -1) ? name : name[0..name.indexOf('<')]);
        }
        return null;
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
                    (neq ? LLVMRealOEQ : LLVMRealONE),
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

        NodeInt[] asInts() {
            return [first.comptime().instanceof!NodeInt,second.comptime().instanceof!NodeInt];
        }

        string getName() {
            NodeIden temp = first.instanceof!NodeIden;
            return temp.name;
        }

        if(operator == TokType.Plus) return new NodeInt(asInts()[0].value+asInts()[1].value);
        if(operator == TokType.Minus) return new NodeInt(asInts()[0].value-asInts()[1].value);
        if(operator == TokType.Multiply) return new NodeInt(asInts()[0].value*asInts()[1].value);
        if(operator == TokType.PluEqu) {
            AliasTable[getName()] = new NodeInt(asInts()[0].value+asInts()[1].value);
        }
        if(operator == TokType.MinEqu) {
            AliasTable[getName()] = new NodeInt(asInts()[0].value-asInts()[1].value);
        }
        if(operator == TokType.Multiply) {
            AliasTable[getName()] = new NodeInt(asInts()[0].value*asInts()[1].value);
        }
        return null;
    }

    override LLVMValueRef generate() {
        import std.algorithm : count;

        bool isAliasIden = false;
        if(NodeIden i = first.instanceof!NodeIden) {isAliasIden = i.name.into(AliasTable);}

        if(operator == TokType.Equ) { // TODO: Replace it to setTo function call
            if(NodeIden i = first.instanceof!NodeIden) {
                if(currScope.getVar(i.name,loc).isConst && i.name != "this" && currScope.getVar(i.name,loc).isChanged) {
                    Generator.error(loc, "An attempt to change the value of a constant variable!");
                }
                if(!currScope.getVar(i.name,loc).isChanged) currScope.hasChanged(i.name);
                if(isAliasIden) {
                    AliasTable[i.name] = second;
                    return null;
                }

                LLVMValueRef val = second.generate();
                LLVMTypeRef ty = LLVMTypeOf(currScope.get(i.name,loc));
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

                if(LLVMTypeOf(val) != ty 
                   && LLVMTypeOf(val) == LLVMPointerType(LLVMInt8TypeInContext(Generator.Context),0)) {
                    val = LLVMBuildBitCast(
                        Generator.Builder,
                        val,
                        ty,
                        toStringz("bitcast")
                    );
                }
                else if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMGetTypeKind(ty) && LLVMGetTypeKind(ty) == LLVMIntegerTypeKind && LLVMTypeOf(val) != ty) {
                    val = LLVMBuildIntCast(Generator.Builder,val,ty,toStringz("intc_"));
                }
                else if(LLVMGetTypeKind(ty) == LLVMDoubleTypeKind && LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMFloatTypeKind) {
                    val = LLVMBuildFPCast(Generator.Builder,val,ty,toStringz("floatc_"));
                }
                else if(LLVMGetTypeKind(ty) == LLVMFloatTypeKind && LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMDoubleTypeKind) {
                    val = LLVMBuildFPCast(Generator.Builder,val,ty,toStringz("floatc_"));
                }
                else if(ty != LLVMTypeOf(val)) {
                    Generator.error(loc,"The variable '"~i.name~"' with type '"~Generator.typeToString(ty)~"' is incompatible with value type '"~Generator.typeToString(LLVMTypeOf(val))~"'!");
                }

                return LLVMBuildStore(
                    Generator.Builder,
                    val,
                    currScope.getWithoutLoad(i.name,loc)
                );
            }
            else if(NodeGet g = first.instanceof!NodeGet) {
                LLVMValueRef val = second.generate();

                if(Generator.typeToString(LLVMTypeOf(val))[0..$-1] == Generator.typeToString(LLVMTypeOf(g.generate()))) {
                    val = LLVMBuildLoad(Generator.Builder,val,toStringz("load764_1_"));
                }

                g.isMustBePtr = true;
                
                if(NodeIden id = g.base.instanceof!NodeIden) {
                    if(id.name.into(VarTable) && currScope.isPure) {
                        Generator.error(loc,"An attempt to change the global variable '"~id.name~"' in a pure function '"~currScope.func~"'!");
                    }
                }

                LLVMValueRef _g = g.generate();
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
                
                LLVMValueRef val = second.generate();

                if(LLVMTypeOf(ptr) == LLVMPointerType(LLVMPointerType(LLVMTypeOf(val),0),0)) ptr = LLVMBuildLoad(Generator.Builder,ptr,toStringz("load784_"));

                if(LLVMGetElementType(LLVMTypeOf(ptr)) != LLVMTypeOf(val) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMGetTypeKind(LLVMTypeOf(val))) {
                    if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind) {
                        val = LLVMBuildIntCast(Generator.Builder,val,LLVMGetElementType(LLVMTypeOf(ptr)),toStringz("intc_"));
                    }
                }

                return LLVMBuildStore(Generator.Builder,val,ptr);
            }
        }

        if(isStatic) {
            if(operator != TokType.Plus && operator != TokType.Minus
             &&operator != TokType.Multiply) return null;
        }

        LLVMValueRef f = first.generate();
        LLVMValueRef s = second.generate();

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
        
        switch(operator) {
            case TokType.PluEqu:
            case TokType.MinEqu:
            case TokType.MulEqu:
            case TokType.DivEqu:
                LLVMValueRef val;
                if(operator == TokType.PluEqu) operator = TokType.Plus;
                else if(operator == TokType.MinEqu) operator = TokType.Minus;
                else if(operator == TokType.MulEqu) operator = TokType.Multiply;
                else if(operator == TokType.DivEqu) operator = TokType.Divide;
                val = mathOperation(f,s);
                if(NodeGet ng = first.instanceof!NodeGet) {
                    ng.isMustBePtr = true;
                    f = ng.generate();
                    return LLVMBuildStore(
                        Generator.Builder,
                        val,
                        f
                    );
                }
                else if(NodeIndex ind = first.instanceof!NodeIndex) {
                    ind.isMustBePtr = true;
                    return LLVMBuildStore(Generator.Builder,val,ind.generate());
                }
                else if(NodeIden id = first.instanceof!NodeIden) {
                    return LLVMBuildStore(
                        Generator.Builder,
                        val,
                        currScope.getWithoutLoad(id.name,loc)
                    );
                }
                else assert(0);
            case TokType.Rem:
                if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMFloatTypeKind) {
                    return LLVMBuildFRem(
                        Generator.Builder,
                        f, 
                        (LLVMGetTypeKind(LLVMTypeOf(s)) == LLVMFloatTypeKind ? s : LLVMBuildSIToFP(Generator.Builder,s,LLVMTypeOf(f),toStringz("sitofp"))),
                        toStringz("fr")
                    );
                }
                else if(LLVMGetTypeKind(LLVMTypeOf(f)) == LLVMIntegerTypeKind) {
                    return LLVMBuildSRem(
                        Generator.Builder,
                        f,
                        (LLVMGetTypeKind(LLVMTypeOf(s)) == LLVMIntegerTypeKind ? s : LLVMBuildFPToSI(Generator.Builder,s,LLVMTypeOf(f),toStringz("fptosi"))),
                        toStringz("sr")
                    );
                }
                break;
            default: break;
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

    override LLVMValueRef generate() {
        for(int i=0; i<nodes.length; i++) {
            LLVMValueRef val = nodes[i].generate();
        }
        return null;
    }

    override void check() {
        foreach(n; nodes) {
            n.check();
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
        else if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
            if(LLVMGetTypeKind(LLVMGetElementType(t)) == LLVMStructTypeKind) return new TypeStruct(cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(t))));
            return new TypePointer(null);
        }
        else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) return new TypeStruct(cast(string)fromStringz(LLVMGetStructName(t)));
        return null;
    }

    override void check() {
        if(namespacesNames.length>0) {
            name = namespacesNamesToString(namespacesNames,name);
        }
        if(isGlobal) VarTable[name] = this;
    }

    override LLVMValueRef generate() {
        if(t.instanceof!TypeAlias) {
            AliasTable[name] = value;
            return null;
        }
        if(t.instanceof!TypeAuto && value is null) Generator.error(loc,"using 'auto' without an explicit value is prohibited!");
        if(currScope is null) {
            // Global variable
            // Only const-values
            bool noMangling = false;

            for(int i=0; i<mods.length; i++) {
                if(mods[i].name == "C") noMangling = true;
                if(mods[i].name == "volatile") isVolatile = true;
            }
            if(!t.instanceof!TypeAuto) {
                if(NodeInt ni = value.instanceof!NodeInt) {
                    ni._isVarVal = t;
                    value = ni;
                }
                Generator.Globals[name] = LLVMAddGlobal(
                    Generator.Module, 
                    Generator.GenerateType(t,loc), 
                    toStringz((noMangling) ? name : Generator.mangle(name,false,false))
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

            currScope.localscope[name] = LLVMBuildAlloca(
                Generator.Builder,
                Generator.GenerateType(t,loc),
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

    override LLVMValueRef generate() {
        if(name in AliasTable) return AliasTable[name].generate();
        if(name.into(Generator.Functions)) return Generator.Functions[name];
        if(!currScope.has(name)) {
            Generator.error(loc,"Unknown identifier \""~name~"\"!");
            return null;
        }
        if(isMustBePtr) return currScope.getWithoutLoad(name,loc);
        return currScope.get(name,loc);
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
                default: data ~= "_b"; break;
            }
        }
        else if(TypePointer tp = t.instanceof!TypePointer) {
            if(TypeStruct ts = tp.instance.instanceof!TypeStruct) {
                //data ~= "_ps-"~ts.name;
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
            case "b": data ~= new TypeBasic("bool"); break;
            case "l": data ~= new TypeBasic("long"); break;
            case "f": data ~= new TypeBasic("float"); break;
            case "d": data ~= new TypeBasic("double"); break;
            case "t": data ~= new TypeBasic("cent"); break;
            case "func": data ~= new TypeFunc(null,null); break;
            case "p": data ~= new TypePointer(null); break;
            default:
                if(_types[i][0] == 's') data ~= new TypeStruct(_types[i][2..$]);
                //else data ~= new TypePointer(new TypeStruct(_types[i][3..$]));
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

    string[] templateNames;
    Type[] _templateTypes;

    LLVMTypeRef[] genParamTypes;

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
    }

    override void check() {
        if(namespacesNames.length>0) {
            name = namespacesNamesToString(namespacesNames,name);
        }
        string toAdd = typesToString(args);
        if(!(name !in FuncTable)) {
            if(typesToString(FuncTable[name].args) == toAdd) checkError(loc,"a function with '"~name~"' name already exists on "~to!string(FuncTable[name].loc+1)~" line!");
            else {
                name ~= toAdd;
            }
        }
        if(type.instanceof!TypeArray) {
            //checkError(loc,"it's impossible to return an array!");
        }
        FuncTable[name] = this;
        for(int i=0; i<block.nodes.length; i++) {
            if(NodeRet r = block.nodes[i].instanceof!NodeRet) {
                r.parent = name;
            }
            block.nodes[i].check();
        }
    }

    LLVMTypeRef* getParameters() {
        LLVMTypeRef[] p;
        for(int i=0; i<args.length; i++) {
            if(args[i].type.instanceof!TypeVarArg) {
                isVararg = true;
                args = args[1..$];
                return getParameters();
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
            else if(!args[i].type.instanceof!TypePointer && !args[i].type.instanceof!TypeConst) {
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
        if(!templateNames.empty && !isTemplate) {
            isExtern = false;
            return null;
        }
        string linkName = Generator.mangle(name,true,false,args);
        if(isMethod) linkName = Generator.mangle(name,true,true,args);

        int callconv = 0; // 0 == C

        NodeBuiltin[string] _builtins;

        if(name == "main") linkName = "main";
        for(int i=0; i<mods.length; i++) {
            switch(mods[i].name) {
                case "C": case "c": linkName = name; break;
                case "vararg": isVararg = true; break;
                case "fastcc": callconv = 1; break;
                case "coldcc": callconv = 2; break;
                case "inline": isInline = true; break;
                case "linkname": linkName = mods[i].value; break;
                case "pure": isPure = true; break;
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

        LLVMTypeRef* parametersG = getParameters();

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

        if(isInline) Generator.addAttribute("alwaysinline",LLVMAttributeFunctionIndex,Generator.Functions[name]);
        if(isTemplatePart || isTemplate) {
            LLVMComdatRef cmr = LLVMGetOrInsertComdat(Generator.Module,toStringz(name));
            LLVMSetComdatSelectionKind(cmr,LLVMAnyComdatSelectionKind);

            LLVMSetComdat(Generator.Functions[name],cmr);
            LLVMSetLinkage(Generator.Functions[name],LLVMLinkOnceODRLinkage);
            isExtern = false;
        }

        if(!isExtern) {
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
        //writeln("MODULE(!!!!!!!!!\n!!!!!!\n!!!!!!\n!!!!!!\n!!!!): ",fromStringz(LLVMPrintModuleToString(Generator.Module)),"\nEnd.");
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

    override void check() {
        if(parent.into(FuncTable)) FuncTable[parent].rets ~= this;
    }

    override LLVMValueRef generate() {
        LLVMValueRef ret;
        if(parent.into(FuncTable)) {
            if(val !is null) {
                if(Generator.ActiveLoops.length != 0) {
                    Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].hasEnd = true;
                    Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].rets ~= LoopReturn(this,cast(int)Generator.ActiveLoops.length-1);
                }
                if(NodeNull n = val.instanceof!NodeNull) {
                    n.maintype = FuncTable[parent].type;
                    LLVMValueRef retval = n.generate();
                    FuncTable[parent].genRets ~= RetGenStmt(Generator.currBB,n.generate());
			        ret = LLVMBuildBr(Generator.Builder, FuncTable[parent].exitBlock);
                    return retval;
                }
                LLVMValueRef retval = val.generate();
                if(Generator.typeToString(LLVMTypeOf(retval))[0..$-1] == Generator.typeToString(Generator.GenerateType(FuncTable[parent].type,loc))) {
                    retval = LLVMBuildLoad(Generator.Builder,retval,toStringz("load1389_"));
                }
                FuncTable[parent].genRets ~= RetGenStmt(Generator.currBB,retval);
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

    LLVMValueRef[] getPredParameters() {
        LLVMValueRef[] params;

        for(int i=0; i<args.length; i++) {
            if(NodeCall nc = args[i].instanceof!NodeCall) {
                if(NodeIden id = nc.func.instanceof!NodeIden) {
                    LLVMTypeRef t = LLVMTypeOf(currScope.get(id.name,loc));
                    params ~= LLVMConstNull(t);
                }
                else if(NodeGet ng = nc.func.instanceof!NodeGet) {
                    NodeGet _ng = ng;
                    _ng.isMustBePtr = true;
                    LLVMTypeRef t = LLVMTypeOf(_ng.generate());
                    if(LLVMGetTypeKind(t) == LLVMPointerTypeKind) t = LLVMGetElementType(t);
                    if(LLVMGetTypeKind(t) == LLVMFunctionTypeKind)t = LLVMGetReturnType(t);
                    
                    params ~= LLVMConstNull(t);
                }
                else {writeln(nc.func); assert(0);}
            }
            else params ~= args[i].generate();
        }

        return params;
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
            }
        }

        return corrected.dup;
    }

    LLVMValueRef[] getParameters(FuncArgSet[] fas = null) {
        LLVMValueRef[] params;

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
            else if(LLVMGetTypeKind(t) == LLVMStructTypeKind) types ~= new TypeStruct(cast(string)fromStringz(LLVMGetStructName(t)));
            else if(LLVMGetTypeKind(t) == LLVMArrayTypeKind) types ~= new TypeArray(0,null);
        }

        return types.dup;
    }

    override LLVMValueRef generate() {
        import lexer.lexer : Lexer;

        if(NodeIden f = func.instanceof!NodeIden) {
        if(!f.name.into(FuncTable) && !into(f.name~to!string(args.length),FuncTable) && !f.name.into(Generator.LLVMFunctions) && !into(f.name~typesToString(parametersToTypes(getParameters())),FuncTable)) {
            if(!f.name.into(MacroTable)) {
                if(currScope.has(f.name)) {
                    NodeVar nv = currScope.getVar(f.name,loc);
                    TypeFunc tf = nv.t.instanceof!TypeFunc;
                    string name = "CallVar"~f.name;
                    return LLVMBuildCall(
                        Generator.Builder,
                        currScope.get(f.name,loc),
                        getParameters().ptr,
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
                        Generator.error(loc,"Unknown macro or function '"~f.name~"'!");
                    }
                    
                    // Template function
                    Lexer l = new Lexer(f.name,1);
                    Parser p = new Parser(l.getTokens(),1,"(builtin)");
                    FuncTable[f.name[0..f.name.indexOf('<')]].generateWithTemplate(p.parseType().instanceof!TypeStruct.types,f.name);
                    return generate();
                }
                Generator.error(loc,"Unknown macro or function '"~f.name~"'!");
            }

            // Generate macro-call
            Scope oldScope = currScope;
            currScope = new Scope(f.name, null, null);
            for(int i=0; i<args.length; i++) {
                currScope.macroArgs[i] = args[i];
            }
            currScope.args = oldScope.args.dup;
            currScope.argVars = oldScope.argVars.dup;
            currScope.localscope = oldScope.localscope.dup;

            Node[] presets;
            presets ~= new NodeVar(
                f.name~"::argCount",
                new NodeInt(currScope.macroArgs.length),
                false,
                true,
                false,
                [],
                loc,
                new TypeAlias()
            );
            presets ~= new NodeVar(
                f.name~"::callLine",
                new NodeInt(loc),
                false,
                true,
                false,
                [],
                loc,
                new TypeAlias()
            );

            for(int i=0; i<MacroTable[f.name].args.length; i++) {
                presets ~= new NodeVar(
                    MacroTable[f.name].args[i],
                    args[i],
                    false,
                    true,
                    false,
                    [],
                    loc,
                    new TypeAlias()
                );
            }

            MacroTable[f.name].block.nodes = presets~MacroTable[f.name].block.nodes;

            MacroTable[f.name].block.generate();
            
            LLVMValueRef toret = null;
            
            if(MacroTable[f.name].ret !is null) {
                MacroTable[f.name].ret.parent = f.name;
                toret = MacroTable[f.name].ret.generate();
            }
            currScope = oldScope;
            return toret;
        }
        if(f.name.into(Generator.LLVMFunctions)) {
            if(FuncTable[f.name].args.length != args.length && FuncTable[f.name].isVararg != true) {
                Generator.error(loc,"The number of arguments in the call does not match the signature!");
                exit(1);
                assert(0);
            }
            calledFunc = FuncTable[f.name];
            LLVMValueRef[] params = getParameters(FuncTable[f.name].args);

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
            return _val;
        }
        LLVMValueRef _val;

        string rname = f.name;
        LLVMValueRef[] params = getPredParameters();
        if(into(f.name~typesToString(parametersToTypes(params)),FuncTable)) {
            rname ~= typesToString(parametersToTypes(params));
            if(rname !in Generator.Functions) {
                Generator.error(loc,"Function '"~rname~"' does not exist!");
            }
            calledFunc = FuncTable[rname];
        }
        else {
            if(rname !in Generator.Functions) {
                Generator.error(loc,"Function '"~rname~"' does not exist!");
            }
            calledFunc = FuncTable[rname];
            params = getParameters(FuncTable[rname].args);
        }
        string name = "CallFunc"~f.name;
        if(FuncTable[rname].type.instanceof!TypeVoid) name = "";
        //writeln("Name: ",rname," with ",typesToString(parametersToTypes(getParameters())));
        if(FuncTable[rname].args.length != args.length && FuncTable[rname].isVararg != true) {
            //writeln(rname," ",FuncTable[rname].args);
            Generator.error(
                loc,
                "The number of arguments in the call("~to!string(args.length)~") does not match the signature("~to!string(FuncTable[rname].args.length)~")!"
            );
            exit(1);
            assert(0);
        }
        if(fromStringz(LLVMPrintValueToString(Generator.Functions[rname])).indexOf("llvm.") != -1) {
            LLVMValueRef _v = LLVMBuildAlloca(Generator.Builder,LLVMInt1TypeInContext(Generator.Context),toStringz("fix"));
            if(Generator.optLevel > 0) LLVMBuildCall(Generator.Builder,Generator.Functions["std::dontuse::_void"],[_v].ptr,1,toStringz(""));
            // This is necessary to solve a bug with LLVM-11
        }
        _val = LLVMBuildCall(
            Generator.Builder,
            Generator.Functions[rname],
            params.ptr,
            cast(uint)args.length,
            toStringz(name)
        );
        if(_inverted) _val = LLVMBuildNot(Generator.Builder,_val,toStringz("not_"));
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
                    LLVMValueRef[] params = getParameters(FuncTable[g.field].args);

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
                    s = Generator.toReplace[s.name].instanceof!TypeStruct;
                }

                if(!into(cast(immutable)[s.name,g.field],MethodTable)) {
                    if(!cast(immutable)[s.name,g.field].into(structsNumbers)) {
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

                    params = getParameters(_args);
                    g.isMustBePtr = true;
                    _toCall = LLVMBuildLoad(Generator.Builder,Generator.byIndex(g.generate(),[LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),structsNumbers[cast(immutable)[s.name,g.field]].number,false)]),toStringz("A"));
                }
                else {
                    calledFunc = MethodTable[cast(immutable)[s.name,g.field]];
                    params = getParameters(MethodTable[cast(immutable)[s.name,g.field]].args);
                }

                //writeln(s.name," ",g.field);
                //params = currScope.get(i.name)~params;
                //writeln("sname: ",s.name," gfield: ",g.field);
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
                    params = getParameters(MethodTable[cast(immutable)[s.name,g.field~typesToString(parametersToTypes(params))]].args);
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
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].args);
                
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
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].args);
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
                LLVMValueRef[] params = getParameters(MethodTable[cast(immutable)[sname,g.field]].args);
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
        element.check();
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
                if(StructTable[structName].operators[TokType.Rbra]["[_i]"].args[0].type.instanceof!TypeStruct) {
                    if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind) ptr = LLVMBuildLoad(Generator.Builder,ptr,toStringz("load_"));
                }
                return LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[StructTable[structName].operators[TokType.Rbra]["[_i]"].name],
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
            Generator.error(loc, "Structure "~struc~" doesn't exist!");
            return null;
        }
        if(!into(cast(immutable)[struc,field],structsNumbers)) {
            if(into(cast(immutable)[struc,field],MethodTable)) {
                return Generator.Functions[MethodTable[cast(immutable)[struc,field]].name];
            }
            else Generator.error(loc,"Structure "~struc~" doesn't contain element "~field~"!");
        }
        if(structsNumbers[cast(immutable)[struc,field]].var.t.instanceof!TypeConst) elementIsConst = true;
        return null;
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
        writeln("Base: ",base);
        assert(0);
    }
}

class NodeBool : Node {
    bool value;

    this(bool value) {
        this.value = value;
    }

    override void check() {

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

    override void check() {base.check();}
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
            if(NodeGet s = base.instanceof!NodeGet) {
                s.isMustBePtr = true;
                return s.generate();
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
                return temp;
            }
            NodeIden id = base.instanceof!NodeIden;
            if(TypeArray ta = currScope.getVar(id.name,loc).t.instanceof!TypeArray) {
                LLVMValueRef ptr = LLVMBuildInBoundsGEP(
					Generator.Builder,
					currScope.getWithoutLoad(id.name),
					[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
					2,
					toStringz("ingep")
				);
                return LLVMBuildPointerCast(
                    Generator.Builder,
                    ptr,
                    Generator.GenerateType(new TypePointer(ta.element),loc),
                    toStringz("bitcast")
                );
            }
            return currScope.getWithoutLoad(id.name);
        }
        else if(type == TokType.Ne) {
            return LLVMBuildNot(Generator.Builder,base.generate(),toStringz("not_"));
        }
        else if(type == TokType.Multiply) {
            return LLVMBuildLoad(
                Generator.Builder,
                base.generate(),
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
                        return new NodeCall(loc,new NodeIden("std::free",loc),[base]).generate();
                    }
                    else return null;
                }
                return new NodeCall(loc,new NodeIden("std::free",loc),[base]).generate();
            }
            return new NodeCall(loc,new NodeIden(StructTable[struc]._destructor.name,loc),[base]).generate();
        }
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

    override void check() {
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

    override Node comptime() {
        NodeBool b = condition.comptime().instanceof!NodeBool;
        if(b.value && _body !is null) _body.generate();
        if(!b.value && _else !is null) _else.generate();
        return null;
    }

    override LLVMValueRef generate() {
        if(isStatic) {
            comptime();
            return null;
        }
        //if(NodeBuiltin _c = condition.instanceof!NodeBuiltin) {
        //    if(_c.generate() != LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false)) {
        //        _body = null;
        //    }
        //}
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

    override void check() {
        if(_body !is null) {
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

    override void check() {}
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

    override void check() {}
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

    this(string name, Node[] nodes, int loc) {
        this.names ~= name;
        this.nodes = nodes.dup;
        this.loc = loc;
    }

    override LLVMValueRef generate() {
        if(currScope !is null) {
            Generator.error(loc,"namespaces cannot be inside functions!");
            return null;
        }
        for(int i=0; i<nodes.length; i++) {
            if(NodeFunc f = nodes[i].instanceof!NodeFunc) {
                if(!f.isExtern) f.isExtern = isImport;
                f.generate();
            }
            else if(NodeVar v = nodes[i].instanceof!NodeVar) {
                if(!v.isExtern) {
                    v.isExtern = isImport;
                }
                v.generate();
            }
            else if(NodeNamespace n = nodes[i].instanceof!NodeNamespace) {
                if(!n.isImport) n.isImport = isImport;
                n.generate();
            }
            else if(NodeStruct s = nodes[i].instanceof!NodeStruct) {
                if(!s.isImported) s.isImported = isImport;
                s.generate();
            }
            else nodes[i].generate();
        }
        return null;
    }

    override void check() {
        for(int i=0; i<nodes.length; i++) {
            if(NodeFunc f = nodes[i].instanceof!NodeFunc) {
                f.namespacesNames ~= names;
                f.check();
            }
            else if(NodeNamespace n = nodes[i].instanceof!NodeNamespace) {
                n.names ~= names;
                n.check();
            }
            else if(NodeVar v = nodes[i].instanceof!NodeVar) {
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
    NodeFunc _this;
    NodeFunc _destructor;
    NodeFunc _with;
    NodeFunc[] methods;
    bool isImported = false;
    string _extends;
    NodeFunc[string][TokType] operators;
    Node[] _oldElements;
    string[] templateNames;

    this(string name, Node[] elements, int loc, string _exs, string[] templateNames) {
        this.name = name;
        this._oldElements = elements.dup;
        this.elements = elements.dup;
        this.loc = loc;
        this.origname = name;
        this._extends = _exs;
        this.templateNames = templateNames.dup;
    }

    LLVMTypeRef asConstType() {
        writeln("CONST: ",name);
        LLVMTypeRef[] types = getParameters(false);
        return LLVMStructType(types.ptr, cast(uint)types.length, false);
    }

    override void check() {
        if(namespacesNames.length>0) {
            name = namespacesNamesToString(namespacesNames,name);
        }
        if(!(name !in StructTable)) {
            checkError(loc,"a struct with that name already exists on "~to!string(StructTable[name].loc+1)~" line!");
        }
        if(_extends != "") {
            NodeStruct _s = StructTable[_extends];
            elements ~= _s.elements;
            methods ~= _s.methods;
            predefines ~= _s.predefines;
        }
        StructTable[name] = this;
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

    void addMethod(NodeFunc f) {
                if(f.name == origname~"(+)" || f.name == origname~"(==)" || f.name == origname~"(!=)" || f.name == origname~"(=)") {
                    f.isMethod = true;
                    if(isImported) f.isExtern = true;

                    TokType oper;
                    switch(f.name[$-3..$]) {
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
                        default: break;
                    }

                    f.name = f.name ~ typesToString(f.args);

                    operators[oper][typesToString(f.args)] = f;
                    methods ~= f;
                    f.check();
                    f.generate();
                }
                else {
                    f.args = FuncArgSet("this",new TypePointer(new TypeStruct(name)))~f.args;
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
                    f.check();
                    f.generate();
                }
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
                if(f.origname == "this") {
                    _this = f;
                    _this.name = origname;
                    _this.type = f.type;
                    _this.namespacesNames = namespacesNames.dup;
                    _this.isTemplatePart = isLinkOnce;

                    if(_this.args.length>0 && _this.args[0].name == "this") _this.args = _this.args[1..$];

                    if(isImported) {
                        _this.isExtern = true;
                        _this.check();
                        continue;
                    }

                    _this.isExtern = false;

                    Node[] toAdd;
                    if((!f.type.instanceof!TypeStruct) && !canFind(f.mods,DeclMod("noNew",""))) {
                        toAdd ~= new NodeBinary(
                            TokType.Equ,
                            new NodeIden("this",_this.loc),
                            new NodeCast(new TypePointer(new TypeStruct(name)),
                                new NodeCall(
                                    _this.loc,new NodeIden("std::malloc",_this.loc),
                                    [new NodeCast(new TypeBasic("int"), new NodeSizeof(new NodeType(new TypeStruct(name),_this.loc),_this.loc), _this.loc)]
                                ), 
                                _this.loc
                            ),
                            _this.loc
                        );
                    }

                    _this.block.nodes = new NodeVar("this",null,false,false,false,[],_this.loc,f.type)~toAdd~_this.block.nodes;
                    _this.check();
                }
                else if(f.origname == "~this") {
                    _destructor = f;
                    _destructor.name = "~"~origname;
                    _destructor.type = f.type;
                    _destructor.isTemplatePart = isLinkOnce;
                    _destructor.namespacesNames = namespacesNames.dup;

                    Type outType = _this.type;
                    if(outType.instanceof!TypeStruct) outType = new TypePointer(outType);

                    if(_destructor.args.length>0 && _destructor.args[0].name == "this") _destructor.args = _destructor.args[1..$];
                    _destructor.args = [FuncArgSet("this",outType)];

                    if(isImported) {
                        _destructor.isExtern = true;
                        _destructor.check();
                        continue;
                    }
                    
                    if(!_this.type.instanceof!TypeStruct) _destructor.block.nodes = _destructor.block.nodes ~ new NodeCall(
                            _destructor.loc,
                            new NodeIden("std::free",_destructor.loc),
                            [new NodeIden("this",_destructor.loc)]
                    );
                    
                    _destructor.check();
                }
                else if(f.origname.indexOf('(') != -1) {
                    f.isMethod = true;
                    f.isTemplatePart = isLinkOnce;
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
                        default: break;
                    }

                    f.name = f.name ~ typesToString(f.args);

                    operators[oper][typesToString(f.args)] = f;
                    methods ~= f;
                }
                else if(f.origname == "~with") {
                    f.isMethod = true;
                    f.isTemplatePart = isLinkOnce;

                    f.name = "~with."~name;

                    Type outType = _this.type;
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
                        outType = _this.type;
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
        if(templateNames.length > 0) return null;

        Generator.Structs[name] = LLVMStructCreateNamed(Generator.Context,toStringz(name));
        LLVMTypeRef[] params = getParameters(isTemplated);
        LLVMStructSetBody(Generator.Structs[name],params.ptr,cast(uint)params.length,false);

        NodeFunc[] _listOfMethods;
        for(int i=0; i<elements.length; i++) {
            if(elements[i].instanceof!NodeFunc) _listOfMethods ~= elements[i].instanceof!NodeFunc;
        }

        if(_destructor is null) {
            _destructor = new NodeFunc("~"~origname,[],new NodeBlock([new NodeCall(loc,new NodeIden("std::free",loc),[new NodeIden("this",loc)])]),false,[],loc,new TypeVoid(),[]);
            _destructor.namespacesNames = namespacesNames.dup;

            if(_this !is null && _this.type.instanceof!TypePointer) {

                if(isImported) {
                    _destructor.isExtern = true;
                    _destructor.check();
                }
                else {
                    Type outType;
                    outType = _this.type;
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
            ///writeln("Structure: ",name,", Type: ",templateNames[i],", replaced: ",_types[i]);
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
        
        NodeStruct _struct = new NodeStruct(name~typesS,copyElements().dup,loc,"",[]);
        _struct.isTemplated = true;
        
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

    override void check() {val.check();}
    override LLVMValueRef generate() {
        LLVMValueRef gval = val.generate();

        if(TypeBasic b = type.instanceof!TypeBasic) {
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
                return LLVMBuildPtrToInt(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(b,loc),
                    toStringz("picast")
                );
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
                    Generator.GenerateType(f),
                    toStringz("ipcast")
                );
            }
            return LLVMBuildPointerCast(
                Generator.Builder,
                gval,
                Generator.GenerateType(f),
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
            NodeBuiltin nb = new NodeBuiltin(b.name,b.args.dup,loc);
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

    override void check() {
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

    override void check() {}

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
                if(var.name.into(Generator.Globals)) Generator.Globals[var.name] = Generator.Globals[oldname];
                else AliasTable[var.name] = AliasTable[oldname];
                //Generator.Globals.remove(oldname);
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
                if(into(f.name,Generator.Functions)) Generator.Functions[f.name] = Generator.Functions[oldname];
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
                //MacroTable.remove(oldname);
            }
        }
        foreach(var; StructTable) {
            if(var.namespacesNames.length>0) {
                string[] newNamespacesNames;
                for(int i=0; i<var.namespacesNames.length; i++) {
                    if(var.namespacesNames[i] != namespace) newNamespacesNames ~= var.namespacesNames[i];
                }
                //writeln(var.origname,":",newNamespacesNames);
                var.namespacesNames = newNamespacesNames.dup;
                string oldname = var.name;
                var.name = namespacesNamesToString(var.namespacesNames, var.origname);
                StructTable[var.name] = var;
                if(var.name.into(Generator.Structs)) Generator.Structs[var.name] = Generator.Structs[oldname];
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

    this() {}

    override void check() {}

    override LLVMValueRef generate() {
        return LLVMConstNull(Generator.GenerateType(maintype));
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

    override void check() {expr.check();}
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
    int loc;

    this(Node I, int loc) {
        this.I = I;
        this.loc = loc;
    }

    override void check() {I.check();}
    override LLVMValueRef generate() {
        return LLVMBuildIntToPtr(
            Generator.Builder,
            I.generate(),
            LLVMPointerType(LLVMInt8TypeInContext(Generator.Context),0),
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

    override void check() {P.check();}
    override LLVMValueRef generate() {
        return LLVMBuildPtrToInt(
            Generator.Builder,
            P.generate(),
            LLVMInt32TypeInContext(Generator.Context),
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

    override void check() {expr.check();}
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

    override void check() {}
    override LLVMValueRef generate() {
        return null;
    }
}

class NodeSizeof : Node {
    Node val;
    int loc;
    
    this(Node val, int loc) {
        this.val = val;
        this.loc = loc;
    }

    override LLVMValueRef generate() {
        if(NodeType ty = val.instanceof!NodeType) {
            return LLVMBuildIntCast(Generator.Builder, LLVMSizeOf(Generator.GenerateType(ty.ty)), LLVMInt32TypeInContext(Generator.Context), toStringz("cast"));
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

    override void check() {/* No there */}

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

    LLVMTypeRef[] generateTypes() {
        LLVMTypeRef[] ts;
        for(int i=0; i<typ.args.length; i++) {
            ts ~= Generator.GenerateType(typ.args[i]);
        }
        return ts.dup;
    }

    override LLVMValueRef generate() {
        type = typ.main;
        LambdaTable["lambda"~to!string(Generator.countOfLambdas)] = this;
        LLVMTypeRef t = LLVMFunctionType(
            Generator.GenerateType(typ.main),
            generateTypes().ptr,
            cast(uint)typ.args.length,
            false
        );

        Generator.countOfLambdas += 1;

        f = LLVMAddFunction(
            Generator.Module,
            toStringz("_RaveL"~to!string(Generator.countOfLambdas-1)),
            t
        );

            LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
                Generator.Context,
                f,
                toStringz("entry")
            );
            
            exitBlock = LLVMAppendBasicBlockInContext(
                Generator.Context,
                f,
                toStringz("exit")
            );

            LLVMBuilderRef oldBuilder = Generator.Builder;
            auto oldLoops = Generator.ActiveLoops;

            builder = LLVMCreateBuilderInContext(Generator.Context);
            Generator.Builder = builder;

            LLVMPositionBuilderAtEnd(
                Generator.Builder,
                entry
            );

            int[string] intargs;
            NodeVar[string] argVars;

            for(int i=0; i<typ.args.length; i++) {
                intargs[typ.args[i].name] = i;
                argVars[typ.args[i].name] = new NodeVar(typ.args[i].name,null,false,true,false,[],loc,typ.args[i].type);
            }

            Scope oldScope = currScope;

            currScope = new Scope("lambda"~to!string(Generator.countOfLambdas-1),intargs.dup,argVars.dup);
            LLVMBasicBlockRef oldCurrBB = Generator.currBB;
            Generator.currBB = entry;

            block.generate();

            if(type.instanceof!TypeVoid && !currScope.funcHasRet) LLVMBuildBr(
                Generator.Builder,
                exitBlock
            );

            LLVMMoveBasicBlockAfter(exitBlock, LLVMGetLastBasicBlock(f));
            LLVMPositionBuilderAtEnd(Generator.Builder, exitBlock);

            if(!type.instanceof!TypeVoid) {
				LLVMValueRef[] retValues = array(map!(x => x.value)(genRets));
				LLVMBasicBlockRef[] retBlocks = array(map!(x => x.where)(genRets));

                if(retBlocks is null || retBlocks.length == 0) {
                    LLVMBuildRet(
                        Generator.Builder,
                        LLVMConstNull(Generator.GenerateType(type))
                    );
                }
				else if(retBlocks.length == 1 || (retBlocks.length>0 && retBlocks[0] == null)) {
					LLVMBuildRet(Generator.Builder, retValues[0]);
				}
				else {
					auto phi = LLVMBuildPhi(Generator.Builder, Generator.GenerateType(type), "retval");
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

            currScope = oldScope;
            Generator.ActiveLoops = oldLoops;
            Generator.Builder = oldBuilder;
            Generator.currBB = oldCurrBB;

        return f;
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

NodeFunc[string] toAddInStructs;

class NodeBuiltin : Node {
    string name;
    Node[] args;
    int loc;
    Type ty;

    this(string name, Node[] args, int loc) {
        this.name = name;
        this.args = args.dup;
        this.loc = loc;
    }

    NodeType asType(int n) {
        if(NodeType nt = args[n].instanceof!NodeType) return nt;
        if(MacroGetArg mga = args[n].instanceof!MacroGetArg) {
            if(NodeIden ni = currScope.macroArgs[mga.number].instanceof!NodeIden) {
                return new NodeType(new TypeStruct(ni.name),loc);
            }
            return currScope.macroArgs[mga.number].instanceof!NodeType;
        }
        if(NodeIden id = args[n].instanceof!NodeIden) {
            if(id.name.into(Generator.toReplace)) return new NodeType(Generator.toReplace[id.name],loc);
            return new NodeType(new TypeBasic(id.name),loc);
        }
        assert(0);
    }

    string asStringIden(int n) {
        if(NodeIden id = args[n].instanceof!NodeIden) return id.name;
        assert(0);
    }

    override void check() {}

    override LLVMValueRef generate() {
        switch(name) {
            case "baseType":
                Type prType = asType(0).ty;
                if(prType.toString().into(Generator.toReplace)) prType = Generator.toReplace[prType.toString()];
                if(TypeArray ta = prType.instanceof!TypeArray) this.ty = ta.element;
                else if(TypePointer tp = prType.instanceof!TypePointer) this.ty = tp.instance;
                else this.ty = prType;
                break;
            case "addMethod": // Deprecated
                if(!asStringIden(0).into(StructTable)) {
                    toAddInStructs[asStringIden(0)] = args[1].instanceof!NodeFunc;
                }
                else StructTable[asStringIden(0)].addMethod(args[1].instanceof!NodeFunc);
                break;
            case "isNumeric":
                Type isNType = asType(0).ty;
                if(isNType.toString().into(Generator.toReplace)) isNType = Generator.toReplace[isNType.toString()];
                if(isNType.toString().into(Generator.toReplace) && Generator.toReplace[isNType.toString()].toString() == isNType.toString()) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                if(isNType.instanceof!TypeBasic) {
                    string s = isNType.toString();
                    if(s == "bool" || s == "char" || s == "int" || s == "short" || s == "long" || s == "cent" || s == "float" || s == "double") {
                        return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                    }
                }
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isFloat":
                Type isNType = asType(0).ty;
                if(isNType.toString().into(Generator.toReplace)) isNType = Generator.toReplace[isNType.toString()];
                if(isNType.toString() == "float" || isNType.toString() == "double") return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isPointer":
                Type isPType = asType(0).ty;
                if(isPType.toString().into(Generator.toReplace)) isPType = Generator.toReplace[isPType.toString()];
                if(isPType.instanceof!TypePointer) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isStructure":
                Type isSType = asType(0).ty;
                if(isSType.toString().into(Generator.toReplace)) isSType = Generator.toReplace[isSType.toString()];
                if(isSType.toString().into(StructTable) || isSType.toString().into(Generator.Structs)) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "isEquals":
                Type isEqType = asType(0).ty;
                Type isEqType2 = asType(1).ty;
                if(isEqType.toString().into(Generator.toReplace)) isEqType = Generator.toReplace[isEqType.toString()];
                if(isEqType2.toString().into(Generator.toReplace)) isEqType2 = Generator.toReplace[isEqType2.toString()];
                if(isEqType.toString() == isEqType2.toString()) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
            case "va_arg":
                return LLVMBuildVAArg(Generator.Builder,args[0].generate(),Generator.GenerateType(asType(1).ty),toStringz("builtInVaArg"));
            case "addLibrary":
                for(int i=0; i<args.length; i++) {
                    if(!canFind(_libraries,args[i].instanceof!NodeString.value)) {
                        _libraries ~= args[i].instanceof!NodeString.value;
                    }
                }
                break;
            case "isConstType":
                if(asType(0).ty.instanceof!TypeConst) return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),1,false);
                return LLVMConstInt(LLVMInt1TypeInContext(Generator.Context),0,false);
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

    this(string line) {
        this.line = line;
    }

    override LLVMValueRef generate() {
        LLVMTypeRef[] ts;
        return LLVMGetInlineAsm(
            LLVMFunctionType(LLVMVoidTypeInContext(Generator.Context),ts.ptr,0,false),
            cast(char*)toStringz(line),
            line.length,
            cast(char*)toStringz(""),
            "".length,
            false,
            false,
            LLVMInlineAsmDialectIntel
        );
    }
}

class NodeMixin : Node { // TODO
    int loc;

    Node[] _strings;

    this(int loc, Node[] _strings) {
        this.loc = loc;
        this._strings = _strings.dup;
    }
}