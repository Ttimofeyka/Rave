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

int countOf(string s, char c) {
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
        writeln("\033[0;31mError on "~to!string(loc+1)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
}

version(linux){extern(C) void LLVMSetGlobalDSOLocal(LLVMValueRef global);}

struct LoopReturn {
    NodeRet ret;
    int loopId;
}

struct Loop {
    bool isActive;
    LLVMBasicBlockRef start;
    LLVMBasicBlockRef end;
    bool hasEnd;
    LoopReturn[] rets;
}

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

    this(string file) {
        this.file = file;
        Context = LLVMContextCreate();
        Module = LLVMModuleCreateWithNameInContext(toStringz("rave"),Context);
        LLVMInitializeAllTargets();
        LLVMInitializeAllAsmParsers();
        LLVMInitializeAllAsmPrinters();
        LLVMInitializeAllTargetInfos();
        LLVMInitializeAllTargetMCs();
    }

    string mangle(string name, bool isFunc, bool isMethod) {
        pragma(inline,true);
        return isFunc ? (isMethod ? "_RaveM"~to!string(name.length)~name : "_RaveF"~to!string(name.length)~name) : "_Raveg"~name;
    }

    void error(int loc,string msg) {
        writeln("\033[0;31mError on "~to!string(loc)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
    }

    LLVMTypeRef GenerateType(Type t) {
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
            LLVMTypeRef a = LLVMPointerType(this.GenerateType(p.instance),0);
            return a;
        }
        if(TypeArray a = t.instanceof!TypeArray) {
            return LLVMArrayType(this.GenerateType(a.element),cast(uint)a.count);
        }
        if(TypeStruct s = t.instanceof!TypeStruct) {
            if(!s.name.into(Generator.Structs)) Generator.error(-1,"Unknown structure '"~s.name~"'!");
            return Generator.Structs[s.name];
        }
        if(t.instanceof!TypeVoid) return LLVMVoidTypeInContext(Generator.Context);
        if(TypeFunc f = t.instanceof!TypeFunc) {
            LLVMTypeRef[] types;
            foreach(ty; f.args) {
                types ~= Generator.GenerateType(ty);
            }
            return 
            LLVMPointerType(
                LLVMFunctionType(
                    Generator.GenerateType(f.main),
                    types.ptr,
                    cast(uint)types.length,
                    false
                ),
                0
            );
        }
        if(t is null) return LLVMPointerType(
            LLVMInt8TypeInContext(Generator.Context),
            0
        );
        assert(0);
    }

    uint getAlignmentOfType(Type t) {
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
            if(StructTable[s.name].elements.length == 0) return 1;
            if(NodeVar v = StructTable[s.name].elements[0].instanceof!NodeVar) return getAlignmentOfType(v.t);
            return 4;
        }
        else if(TypeFunc f = t.instanceof!TypeFunc) {
            return getAlignmentOfType(f.main);
        }
        else if(TypeVoid v = t.instanceof!TypeVoid) {
            return 0;
        }
        assert(0);
    }

    string typeToString(LLVMTypeRef t) {
        return to!string(fromStringz(LLVMPrintTypeToString(t)));
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

    this(string func, int[string] args, NodeVar[string] argVars) {
        this.func = func;
        this.args = args.dup;
        this.argVars = argVars.dup;
    }

    LLVMValueRef getWithoutLoad(string n) {
        if(!(n !in localscope)) return localscope[n];
        if(!(n !in Generator.Globals)) return Generator.Globals[n];
        /*writeln(
            fromStringz(
                LLVMPrintTypeToString(
                    LLVMTypeOf(
                        LLVMGetParam(
                            Generator.Functions[func],
                            cast(uint)args[n]
                        )
                    )
                )
            )
        );*/
        if(!n.into(args)) {
            Generator.error(-1,"Undefined argument '"~n~"'!");
        }
        return LLVMGetParam(
            Generator.Functions[func],
            cast(uint)args[n]
        );
    }

    LLVMValueRef get(string n) {
        if(!(n !in AliasTable)) return AliasTable[n].generate();
        if(!(n !in localscope)) {
            auto instr = LLVMBuildLoad(
                Generator.Builder,
                localscope[n],
                toStringz("loadLocal"~n)
            );
            try {
                LLVMSetAlignment(instr,Generator.getAlignmentOfType(localVars[n].t));
            } catch(Error e) {}
            return instr;
        }
        if(!(n !in Generator.Globals)) {
            auto instr = LLVMBuildLoad(
                Generator.Builder,
                Generator.Globals[n],
                toStringz("loadGlobal"~n)
            );
            LLVMSetAlignment(instr,Generator.getAlignmentOfType(VarTable[n].t));
            return instr;
        }
        return LLVMGetParam(
            Generator.Functions[func],
            cast(uint)args[n]
        );
    }

    NodeVar getVar(string n, int line) {
        if(n.into(localscope)) return localVars[n];
        if(n.into(Generator.Globals)) return VarTable[n];
        if(n.into(args)) return argVars[n];
        Generator.error(line-1,"Undefined variable \""~n~"\"!");
        assert(0);
    }

    bool has(string n) {
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
    private ulong value;
    BasicType ty;

    this(ulong value) {
        this.value = value;
    }

    override LLVMValueRef generate() {
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

    LLVMValueRef sum(LLVMValueRef one, LLVMValueRef two) {
        if(LLVMGetTypeKind(LLVMTypeOf(one)) != LLVMGetTypeKind( LLVMTypeOf(two))) {
            Generator.error(loc,"value types are incompatible!");
        }
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
            return LLVMBuildFAdd(
                Generator.Builder,
                one, two,
                toStringz("FloatSum")
            );
        }
        return LLVMBuildAdd(
            Generator.Builder,
            one, two,
            toStringz("Sum")
        );
    }

    LLVMValueRef sub(LLVMValueRef one, LLVMValueRef two) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            Generator.error(loc,"value types are incompatible!");
        }
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
            return LLVMBuildFSub(
                Generator.Builder,
                one, two,
                toStringz("FloatSub")
            );
        }
        return LLVMBuildSub(
            Generator.Builder,
            one, two,
            toStringz("Sub")
        );
    }

    LLVMValueRef mul(LLVMValueRef one, LLVMValueRef two) {
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
            return LLVMBuildFMul(
                Generator.Builder,
                one, two,
                toStringz("FloatMul")
            );
        }
        return LLVMBuildMul(
            Generator.Builder,
            one, two,
            toStringz("Mul")
        );
    }

    LLVMValueRef div(LLVMValueRef one, LLVMValueRef two) {
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
            return LLVMBuildFDiv(
                Generator.Builder,
                one, two,
                toStringz("FloatDiv")
            );
        }
        return LLVMBuildSDiv(
            Generator.Builder,
            one, two,
            toStringz("Div")
        );
    }

    LLVMValueRef equal(LLVMValueRef onev, LLVMValueRef twov, bool neq) {
        LLVMTypeRef one = LLVMTypeOf(onev);
        LLVMTypeRef two = LLVMTypeOf(twov);
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
            else if(LLVMGetTypeKind(one) == LLVMFloatTypeKind) {
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
        assert(0);
    }

    LLVMValueRef diff(LLVMValueRef one, LLVMValueRef two, bool less, bool orequal) {
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
            else if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
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

    LLVMValueRef or(LLVMValueRef one, LLVMValueRef two) {
        return LLVMBuildOr(
            Generator.Builder,
            one,
            two,
            toStringz("or")
        );
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

        if(operator == TokType.Equ) {
            if(NodeIden i = first.instanceof!NodeIden) {
                if(currScope.getVar(i.name,loc).isConst && i.name != "this") {
                    Generator.error(loc, "An attempt to change the value of a constant variable!");
                }
                if(isAliasIden) {
                    AliasTable[i.name] = second;
                    return null;
                }

                LLVMValueRef val = second.generate();
                LLVMTypeRef ty = LLVMTypeOf(currScope.get(i.name));

                if(LLVMTypeOf(val) != ty 
                   && LLVMTypeOf(val) == LLVMPointerType(LLVMVoidTypeInContext(Generator.Context),0)) {
                    val = LLVMBuildPointerCast(
                        Generator.Builder,
                        val,
                        ty,
                        toStringz("ptrcast")
                    );
                }

                if(Generator.typeToString(LLVMTypeOf(currScope.getWithoutLoad(i.name))) == Generator.typeToString(LLVMTypeOf(val))) {
                    val = LLVMBuildLoad(Generator.Builder,val,toStringz("load"));
                }

                return LLVMBuildStore(
                    Generator.Builder,
                    val,
                    currScope.getWithoutLoad(i.name)
                );
            }
            else if(NodeGet g = first.instanceof!NodeGet) {
                return LLVMBuildStore(
                    Generator.Builder,
                    second.generate(),
                    g.generate()
                );
            }
            else if(NodeIndex ind = first.instanceof!NodeIndex) {
                ind.isEqu = true;
                ind.toSet = second.generate();
                return ind.generate();
            }
        }

        isStatic = isAliasIden;

        if(isStatic) {
            if(operator != TokType.Plus && operator != TokType.Minus
             &&operator != TokType.Multiply) return null;
        }

        LLVMValueRef f = first.generate();
        LLVMValueRef s = second.generate();

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
                if(operator == TokType.PluEqu) val=sum(f,s);
                else if(operator == TokType.MinEqu) val=sub(f,s);
                else if(operator == TokType.MulEqu) val=mul(f,s);
                else if(operator == TokType.DivEqu) val=div(f,s);
                if(NodeGet ng = first.instanceof!NodeGet) {
                    ng.isEqu = true;
                    f = ng.generate();
                    return LLVMBuildStore(
                        Generator.Builder,
                        val,
                        f
                    );
                }
                else if(NodeIndex ind = first.instanceof!NodeIndex) {
                    ind.isEqu = true;
                    ind.toSet = val;
                    return ind.generate();
                }
                else if(NodeIden id = first.instanceof!NodeIden) {
                    return LLVMBuildStore(
                        Generator.Builder,
                        val,
                        currScope.getWithoutLoad(id.name)
                    );
                }
                break;
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
            if(kone == LLVMFloatTypeKind && ktwo == LLVMIntegerTypeKind) s = LLVMBuildSIToFP(
                Generator.Builder,
                s,
                LLVMTypeOf(f),
                toStringz("BinItoF")
            );
            else if(kone == LLVMIntegerTypeKind && ktwo == LLVMFloatTypeKind) {
                f = LLVMBuildSIToFP(
                    Generator.Builder,
                    f,
                    LLVMTypeOf(s),
                    toStringz("BinItoF")
                );
            }
            else {
                writeln(Generator.typeToString(LLVMTypeOf(f)));
                writeln(Generator.typeToString(LLVMTypeOf(s)));
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

        LLVMValueRef and(LLVMValueRef one, LLVMValueRef two) {
            return LLVMBuildAnd(
                Generator.Builder,
                one,
                two,
                toStringz("and")
            );
        }

        if(operator == TokType.Plus) return sum(f,s);
        if(operator == TokType.Minus) return sub(f,s);
        if(operator == TokType.Multiply) return mul(f,s);
        if(operator == TokType.Divide) return div(f,s);
        if(operator == TokType.Equal) return equal(f,s,false);
        if(operator == TokType.Nequal) return equal(f,s,true);
        if(operator == TokType.Less) return diff(f,s,true,false);
        if(operator == TokType.More) return diff(f,s,false,false);
        if(operator == TokType.LessEqual) return diff(f,s,true,true);
        if(operator == TokType.MoreEqual) return diff(f,s,false,true);
        if(operator == TokType.And) return and(f,s);
        if(operator == TokType.Or) return or(f,s);
        if(operator == TokType.BitLeft) return LLVMBuildShl(
            Generator.Builder,
            f,
            s,
            toStringz("bitleft")
        );
        if(operator == TokType.BitRight) return LLVMBuildLShr(
            Generator.Builder,
            f,
            s,
            toStringz("bitright")
        );
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

    this(string name, Node value, bool isExt, bool isConst, bool isGlobal, DeclMod[] mods, int loc, Type t) {
        this.name = name;
        this.origname = name;
        this.value = value;
        this.isExtern = isExt;
        this.isConst = isConst;
        this.mods = mods.dup;
        this.loc = loc;
        this.t = t;
        this.isGlobal = isGlobal;
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
        if(currScope is null) {
            // Global variable
            // Only const-values
            bool noMangling = false;
            for(int i=0; i<mods.length; i++) {
                if(mods[i].name == "C") noMangling = true;
            }
            Generator.Globals[name] = LLVMAddGlobal(
                Generator.Module, 
                Generator.GenerateType(t), 
                toStringz((noMangling) ? name : Generator.mangle(name,false,false))
            );
            if(isExtern) {
                LLVMSetLinkage(Generator.Globals[name], LLVMExternalLinkage);
            }
            // else LLVMSetLinkage(Generator.Globals[name],LLVMCommonLinkage);
            LLVMSetAlignment(Generator.Globals[name],Generator.getAlignmentOfType(t));
            version(linux) {LLVMSetGlobalDSOLocal(Generator.Globals[name]);}
            if(value !is null && !isExtern) {
                LLVMSetInitializer(Generator.Globals[name], value.generate());
            }
            else if(!isExtern) {
                LLVMSetInitializer(Generator.Globals[name], LLVMConstNull(Generator.GenerateType(t)));
            }
        }
        else {
            // Local variables
            currScope.localVars[name] = this;
            currScope.localscope[name] = LLVMBuildAlloca(
                Generator.Builder,
                Generator.GenerateType(t),
                toStringz(name)
            );

            structSetPredefines(t, currScope.localscope[name]);

            LLVMSetAlignment(currScope.getWithoutLoad(name),Generator.getAlignmentOfType(t));
            
            if(value !is null) LLVMBuildStore(
                Generator.Builder,
                value.generate(),
                currScope.getWithoutLoad(name)
            ).LLVMSetAlignment(Generator.getAlignmentOfType(t));
        }
        return null;
    }

    override void debugPrint() {
        writeln("NodeVar("~name~",isExtern="~to!string(isExtern)~")");
    }
}

class NodeIden : Node {
    string name;
    int loc;

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
        return currScope.get(name);
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

    this(string name, FuncArgSet[] args, NodeBlock block, bool isExtern, DeclMod[] mods, int loc, Type type) {
        this.name = name;
        this.origname = name;
        this.args = args;
        this.block = block;
        this.isExtern = isExtern;
        this.mods = mods.dup;
        this.loc = loc;
        this.type = type;
    }

    override void check() {
        if(namespacesNames.length>0) {
            name = namespacesNamesToString(namespacesNames,name);
        }
        if(!(name !in FuncTable)) {
            if(args.length != FuncTable[name].args.length) name ~= to!string(args.length);
            else checkError(loc,"a function with '"~name~"' name already exists on "~to!string(FuncTable[name].loc+1)~" line!");
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
            else p ~= Generator.GenerateType(args[i].type);
        }
        return p.ptr;
    }

    override LLVMValueRef generate() {
        string linkName = Generator.mangle(name,true,false);
        if(isMethod) linkName = Generator.mangle(name,true,true);

        int callconv = 0; // 0 == C

        if(name == "main") linkName = "main";

        for(int i=0; i<mods.length; i++) {
            if(mods[i].name == "C") linkName = name;
            else if(mods[i].name == "vararg") isVararg = true;
            else if(mods[i].name == "linkname") linkName = mods[i].value;
            else if(mods[i].name == "fastcc") callconv = 1;
            else if(mods[i].name == "coldcc") callconv = 2;
        }

        LLVMTypeRef functype = LLVMFunctionType(
            Generator.GenerateType(type),
            getParameters(),
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

            builder = LLVMCreateBuilder();
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
            Generator.currBB = entry;

            block.generate();

            if(type.instanceof!TypeVoid && !currScope.funcHasRet) LLVMBuildBr(
                Generator.Builder,
                exitBlock
            );

            LLVMMoveBasicBlockAfter(exitBlock, LLVMGetLastBasicBlock(Generator.Functions[name]));
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
			
			Generator.Builder = null;
            currScope = null;
        }

        version(linux) {LLVMSetGlobalDSOLocal(Generator.Functions[name]);}

        //writeln(fromStringz(LLVMPrintModuleToString(Generator.Module)));

        LLVMVerifyFunction(Generator.Functions[name],0);

        return Generator.Functions[name];
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
                FuncTable[parent].genRets ~= RetGenStmt(Generator.currBB,retval);
			    ret = LLVMBuildBr(Generator.Builder, FuncTable[parent].exitBlock);
                return retval;
            }
            else ret = LLVMBuildBr(Generator.Builder,FuncTable[parent].exitBlock);
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

    this(int loc, Node func, Node[] args) {
        this.loc = loc;
        this.func = func;
        this.args = args.dup;
    }

    LLVMValueRef[] getParameters() {
        LLVMValueRef[] params;
        for(int i=0; i<args.length; i++) {
            LLVMValueRef arg = args[i].generate();
            params ~= arg;
        }
        return params.dup;
    }

    LLVMValueRef[] getNewParameters(FuncArgSet[] a) {
        LLVMValueRef[] params;
        for(int i=0; i<args.length; i++) {
            LLVMValueRef arg = args[i].generate();
            if(LLVMTypeOf(arg) == LLVMInt32TypeInContext(Generator.Context)) {
                if(TypeBasic b = a[i].type.instanceof!TypeBasic) {
                    if(b.type != BasicType.Int) {
                        // More or less
                        arg = LLVMBuildIntCast(
                            Generator.Builder,
                            arg,
                            Generator.GenerateType(a[i].type),
                            toStringz("itoi")
                        );
                    }
                }
            }
            if(LLVMTypeOf(arg) == LLVMPointerType(LLVMVoidTypeInContext(Generator.Context),0)) {
                if(TypePointer p = a[i].type.instanceof!TypePointer) {
                    if(p.instance != new TypeVoid()) arg = LLVMBuildPointerCast(
                        Generator.Builder,
                        arg,
                        Generator.GenerateType(a[i].type),
                        toStringz("ptop")
                    );
                }
            }
            params ~= arg;
        }
        return params.dup;
    }

    override LLVMValueRef generate() {
        if(NodeIden f = func.instanceof!NodeIden) {
        if(!f.name.into(FuncTable) && !into(f.name~to!string(args.length),FuncTable) && !f.name.into(Generator.LLVMFunctions)) {
            if(!f.name.into(MacroTable)) {
                if(currScope.has(f.name)) {
                    LLVMTypeRef a = LLVMTypeOf(currScope.get(f.name));
                    string name = "CallVar"~f.name;
                    if(LLVMGetTypeKind(a) == LLVMVoidTypeKind) name = "";
                    return LLVMBuildCall(
                        Generator.Builder,
                        currScope.get(f.name),
                        getParameters().ptr,
                        cast(uint)args.length,
                        toStringz(name)
                    );
                }
                Generator.error(loc,"Unknown macro or function '"~f.name~"'!");
                exit(1);
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
            if(Generator.LLVMFunctionTypes[f.name] != LLVMVoidTypeInContext(Generator.Context)) return LLVMBuildCall(
                Generator.Builder,
                Generator.LLVMFunctions[f.name],
                getNewParameters(FuncTable[f.name].args).ptr,
                cast(uint)args.length,
                toStringz("CallFunc"~f.name)
            );
            return LLVMBuildCall(
                Generator.Builder,
                Generator.LLVMFunctions[f.name],
                getNewParameters(FuncTable[f.name].args).ptr,
                cast(uint)args.length,
                toStringz("")
            );
        }
        string rname = f.name;
        if(into(f.name~to!string(args.length),FuncTable)) rname ~= to!string(args.length);
        string name = "CallFunc"~f.name;
        if(FuncTable[rname].type.instanceof!TypeVoid) name = "";
        if(FuncTable[rname].args.length != args.length && FuncTable[rname].isVararg != true) {
            Generator.error(
                loc,
                "The number of arguments in the call("~to!string(args.length)~") does not match the signature("~to!string(FuncTable[rname].args.length)~")!"
            );
            exit(1);
            assert(0);
        }
        return LLVMBuildCall(
            Generator.Builder,
            Generator.Functions[rname],
            getParameters().ptr,
            cast(uint)args.length,
            toStringz(name)
        );
        }
        else if(NodeGet g = func.instanceof!NodeGet) {
            if(NodeIden i = g.base.instanceof!NodeIden) {
                TypeStruct s;
                if(TypePointer p = currScope.getVar(i.name,loc).t.instanceof!TypePointer) {
                    s = p.instance.instanceof!TypeStruct;
                }
                else s = currScope.getVar(i.name,loc).t.instanceof!TypeStruct;
                LLVMValueRef[] params = getParameters();
                params = currScope.get(i.name)~params;
                //writeln("sname: ",s.name," gfield: ",g.field);
                return LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[s.name,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[s.name,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
            }
            else if(NodeIndex ni = g.base.instanceof!NodeIndex) {
                auto val = ni.generate();
                string sname = Generator.typeToString(LLVMTypeOf(val))[1..$-1];
                LLVMValueRef[] params = getParameters();
                params = val~params;
                
                return LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );

                //Generator.error(loc,"TODO!");
            }
            else if(NodeGet ng = g.base.instanceof!NodeGet) {
                auto val = ng.generate();
                string sname = Generator.typeToString(LLVMTypeOf(val))[1..$-1];
                LLVMValueRef[] params = getParameters();
                params = val~params;
                
                return LLVMBuildCall(
                    Generator.Builder,
                    Generator.Functions[MethodTable[cast(immutable)[sname,g.field]].name],
                    params.ptr,
                    cast(uint)params.length,
                    toStringz(MethodTable[cast(immutable)[sname,g.field]].type.instanceof!TypeVoid ? "" : g.field)
                );
            }
            else {
                writeln(g.base);
                Generator.error(loc,"TODO!");
            }
        }
        assert(0);
    }
    

    override void debugPrint() {
        writeln("NodeCall()");
    }
}

class NodeIndex : Node {
    Node element;
    LLVMValueRef toSet;
    Node[] indexs;
    int loc;
    bool isEqu = false;

    this(Node element, Node[] indexs, int loc) {
        this.element = element;
        this.indexs = indexs.dup;
        this.loc = loc;
    }

    override void check() {element.check();}
    override LLVMValueRef generate() {
        LLVMValueRef[] genIndexs;
        for(int i=0; i<indexs.length; i++) {
            genIndexs ~= indexs[i].generate();
        }
        if(!isEqu) {
            if(NodeIden id = element.instanceof!NodeIden) {
                if(currScope.getVar(id.name,loc).t.instanceof!TypePointer) {
                    return LLVMBuildLoad(Generator.Builder, LLVMBuildGEP(
                        Generator.Builder,
                        currScope.get(id.name),
                        genIndexs.ptr,
                        cast(uint)genIndexs.length,
                        toStringz("gep")
                    ), toStringz("load"));
                }
            }
            NodeGet g = element.instanceof!NodeGet;
            
            return LLVMBuildLoad(
                Generator.Builder,
                LLVMBuildGEP(
                    Generator.Builder,
                    g.generate(),
                    genIndexs.ptr,
                    cast(uint)genIndexs.length,
                    toStringz("gep")
                ),
                toStringz("load")
            );
        }
        else { if(NodeIden id = element.instanceof!NodeIden) {
            if(currScope.getVar(id.name,loc).t.instanceof!TypePointer) {
                return LLVMBuildStore(Generator.Builder, toSet, LLVMBuildGEP(
                    Generator.Builder,
                    currScope.get(id.name),
                    genIndexs.ptr,
                    cast(uint)genIndexs.length,
                    toStringz("gep")
                ));
            }
        }
        else if(NodeGet g = element.instanceof!NodeGet) {
            LLVMValueRef ptr = LLVMBuildGEP(
                    Generator.Builder,
                    g.generate(),
                    genIndexs.ptr,
                    cast(uint)genIndexs.length,
                    toStringz("gep")
            );
            return LLVMBuildStore(
                Generator.Builder,
                toSet,
                ptr
            );
        }
    }

    NodeIden id = element.instanceof!NodeIden;

    // Arrays
    genIndexs = LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),0,false)~genIndexs;
    if(!isEqu) {
        return LLVMBuildLoad(
            Generator.Builder,
            LLVMBuildInBoundsGEP(
                Generator.Builder,
                currScope.getWithoutLoad(id.name),
                genIndexs.ptr,
                cast(uint)genIndexs.length,
                toStringz("gep")
            ),
            toStringz("load2")
        );
    }
    return LLVMBuildStore(
        Generator.Builder,
        toSet,
        LLVMBuildInBoundsGEP(
            Generator.Builder,
            currScope.getWithoutLoad(id.name),
            genIndexs.ptr,
            cast(uint)genIndexs.length,
            toStringz("ingep")
        )
    );

    }
}

StructMember[string[]] structsNumbers;

class NodeGet : Node {
    Node base;
    string field;
    int loc;
    bool isEqu;

    this(Node base, string field, bool isEqu, int loc) {
        this.base = base;
        this.field = field;
        this.loc = loc;
        this.isEqu = isEqu;
    }

    // A, B, C, D
    // ((test),test2),(test3)),test4

    TypeStruct getDownStruct(TypePointer p) {
        Type s = p.instance;
        while(true) {
            if(s.instanceof!TypeStruct) return s.instanceof!TypeStruct;
            else if(TypePointer tp = s.instanceof!TypePointer) {
                s = tp.instance;
            }
            else if(TypeArray ta = s.instanceof!TypeArray) {
                s = ta.element;
            }
        }
    }

    TypeStruct getStruct() {
        if(NodeIden id = base.instanceof!NodeIden) {
            if(TypePointer p = currScope.getVar(id.name,loc).t.instanceof!TypePointer) {
                return p.instance.instanceof!TypeStruct;
            }
            return currScope.getVar(id.name,loc).t.instanceof!TypeStruct;
        }
        else {
            NodeGet g = base.instanceof!NodeGet;
            TypeStruct prestruct = g.getStruct();
            StructMember member = structsNumbers[cast(immutable)[prestruct.name,g.field]];
            NodeVar parentvar = member.var;

            TypeStruct ts;

            if(TypePointer tp = parentvar.t.instanceof!TypePointer) {
                ts = getDownStruct(tp);
            }
            else ts = parentvar.t.instanceof!TypeStruct;

            return ts;
        }
    }

    bool isStructGet() {
        if(NodeIden id = base.instanceof!NodeIden) {
            if(currScope.getVar(id.name,loc).t.instanceof!TypeStruct) return true;
            return false;
        }
        NodeGet g = base.instanceof!NodeGet;
        return g.isStructGet();
    }

    LLVMValueRef generateForParent() {
        if(NodeIden id = base.instanceof!NodeIden) {
            TypeStruct s = currScope.getVar(id.name,loc).t.instanceof!TypeStruct;
            return LLVMBuildStructGEP(
                Generator.Builder,
                currScope.getWithoutLoad(id.name),
                structsNumbers[cast(immutable)[s.name,field]].number,
                toStringz("getStructElement"~field)
            );
        }
        assert(0);
    }

    string getVariable(NodeGet g) {
        NodeGet ng = g.base.instanceof!NodeGet;
        while(true) {
            if(ng is null) {
                NodeIden id = ng.base.instanceof!NodeIden;
                return id.name;
            }
            ng = ng.base.instanceof!NodeGet;
        }
        assert(0); // Compability
    }

    override LLVMValueRef generate() {
        if(NodeIden id = base.instanceof!NodeIden) {
            TypeStruct s;
            LLVMValueRef ptr = currScope.getWithoutLoad(id.name);

            string t = Generator.typeToString(LLVMTypeOf(ptr));
            if(t[$-1] == '*' && t[$-2] == '*') {
                ptr = LLVMBuildLoad(
                    Generator.Builder,
                    ptr,
                    toStringz("load")
                );
            }

            if(TypeStruct ts = currScope.getVar(id.name,loc).t.instanceof!TypeStruct) {s = ts;}
            else {
                if(TypePointer p = currScope.getVar(id.name,loc).t.instanceof!TypePointer) {
                    if(TypeStruct ts = p.instance.instanceof!TypeStruct) {
                        s = ts;
                    }
                    else Generator.error(loc,"This isn't a structure!");
                }
                else Generator.error(loc,"This isn't a structure!");
            }

            if(!(cast(immutable)[s.name,field].into(structsNumbers))) {
                Generator.error(loc,"Element "~field~" does not exist!");
            }
            if(isEqu) return LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                structsNumbers[cast(immutable)[s.name,field]].number,
                toStringz("sgep")
            );
            return LLVMBuildLoad(Generator.Builder, LLVMBuildStructGEP(
                Generator.Builder,
                ptr,
                structsNumbers[cast(immutable)[s.name,field]].number,
                toStringz("sgep")
            ), toStringz("load"));
        }
        else if(NodeGet g = base.instanceof!NodeGet) {
            g.isEqu = true;

            TypeStruct s = getStruct();

            LLVMValueRef gg = g.generate();

            if(Generator.typeToString(LLVMTypeOf(gg)).countOf('*') > 1) {
                gg = LLVMBuildLoad(
                    Generator.Builder,
                    gg,
                    toStringz("load")
                );
            }
            
            if(isEqu) return LLVMBuildStructGEP(
                Generator.Builder,
                //g.generate(),
                gg,
                structsNumbers[cast(immutable)[s.name,field]].number,
                toStringz("getStructElement"~field)
            );

            return LLVMBuildLoad(
                Generator.Builder,
                LLVMBuildStructGEP(
                    Generator.Builder,
                    g.generate(),
                    structsNumbers[cast(immutable)[s.name,field]].number,
                    toStringz("getStructElement"~field)
                ),
                toStringz("loadGEP")
            );
        }
        else if(NodeUnary u = base.instanceof!NodeUnary) {
            writeln(field);
        }
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

    override void check() {base.check();}
    override LLVMValueRef generate() {
        if(type == TokType.Minus) return LLVMBuildNeg(
            Generator.Builder,
            base.generate(),
            toStringz("neg")
        );
        else if(type == TokType.GetPtr) {
            if(NodeGet s = base.instanceof!NodeGet) {
                s.isEqu = true;
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
            if(currScope.getVar(id.name,loc).t.instanceof!TypeArray) {
                return LLVMBuildInBoundsGEP(
					Generator.Builder,
					currScope.getWithoutLoad(id.name),
					[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
					2,
					toStringz("ingep")
				);
            }
            return currScope.getWithoutLoad(id.name);
        }
        else if(type == TokType.Ne) {
            return LLVMBuildNeg(
                Generator.Builder,
                base.generate(),
                toStringz("ne")
            );
        }
        else if(type == TokType.Multiply) {
            return LLVMBuildLoad(
                Generator.Builder,
                base.generate(),
                toStringz("load")
            );
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
                    else if(NodeBreak br = _body.instanceof!NodeBreak) br.fromIf = true;
                }
            }
            else if(NodeBreak br = _body.instanceof!NodeBreak) {
                br.fromIf = true;
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
        Generator.ActiveLoops[selfNum] = Loop(true,thenBlock,endBlock,false);

        LLVMPositionBuilderAtEnd(Generator.Builder, thenBlock);
        Generator.currBB = thenBlock;
        if(_body !is null) _body.generate();
        if(!Generator.ActiveLoops[selfNum].hasEnd) LLVMBuildBr(Generator.Builder,endBlock);

        if(Generator.ActiveLoops[selfNum].rets.length>0) {
            FuncTable[func].rets ~= Generator.ActiveLoops[selfNum].rets[0].ret;
        }
        
        bool hasEnd1 = Generator.ActiveLoops[selfNum].hasEnd;

        Generator.ActiveLoops[selfNum] = Loop(true,elseBlock,endBlock,false);

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
                LLVMBuildRet(Generator.Builder,LLVMConstNull(Generator.GenerateType(FuncTable[func].type)));
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
        Generator.ActiveLoops[selfNumber] = Loop(true,condBlock,currScope.BlockExit,false);
        
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
    bool fromIf = false;

    this(int loc) {this.loc = loc;}

    override void check() {}
    override LLVMValueRef generate() {
        LLVMBuildBr(
            Generator.Builder,
            Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].end
        );
        if(fromIf) return null;
        Generator.ActiveLoops[cast(int)Generator.ActiveLoops.length-1].hasEnd = true;
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
    NodeFunc[] methods;
    bool isImported = false;

    this(string name, Node[] elements, int loc) {
        this.name = name;
        this.elements = elements.dup;
        this.loc = loc;
        this.origname = name;
    }

    LLVMTypeRef asConstType() {
        LLVMTypeRef[] types = getParameters();
        return LLVMStructType(types.ptr, cast(uint)types.length, false);
    }

    override void check() {
        if(namespacesNames.length>0) {
            name = namespacesNamesToString(namespacesNames,name);
        }
        if(!(name !in StructTable)) {
            checkError(loc,"a struct with that name already exists on "~to!string(StructTable[name].loc+1)~" line!");
        }
        StructTable[name] = this;
    }

    bool hasPredefines() {
        for(int i=0; i<elements.length; i++) {
            if(NodeVar v = elements[i].instanceof!NodeVar) {
                if(TypeStruct s = v.t.instanceof!TypeStruct) {
                    if(StructTable[s.name].hasPredefines()) return true;
                }
                else {
                    if(v.value !is null) return true;
                }
            }
        }
        return false;
    }

    LLVMTypeRef[] getParameters() {
        LLVMTypeRef[] values;
        for(int i=0; i<elements.length; i++) {
            if(NodeVar v = elements[i].instanceof!NodeVar) {
                structsNumbers[cast(immutable)[name,v.name]] = StructMember(i,v);
                if(v.value !is null) {
                    predefines ~= StructPredefined(i,v.value,false);
                }
                else if(TypeStruct ts = v.t.instanceof!TypeStruct) {
                    if(StructTable[ts.name].hasPredefines()) predefines ~= StructPredefined(i,v,true);
                }
                else predefines ~= StructPredefined(i,null,false);
                values ~= Generator.GenerateType(v.t);
            }
            else {
                NodeFunc f = elements[i].instanceof!NodeFunc;
                if(f.name == "this") {

                    if(isImported) {
                        _this = f;
                        _this.isExtern = true;
                        _this.name = origname;
                        _this.namespacesNames = namespacesNames.dup;
                        _this.check();
                        continue;
                    }

                    _this = f;
                    _this.isExtern = false;
                    _this.name = origname;
                    _this.namespacesNames = namespacesNames.dup;

                    NodeBinary b = new NodeBinary(
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

                    Node[] olds = _this.block.nodes.dup;
                    Node[] none;
                    _this.block.nodes = none.dup;

                    _this.block.nodes ~= new NodeVar("this",null,false,false,false,[],_this.loc,new TypePointer(new TypeStruct(name)));
                    _this.block.nodes ~= b;
                    _this.block.nodes ~= olds.dup;

                    _this.check();
                }
                else {
                    f.args = FuncArgSet("this",new TypePointer(new TypeStruct(name)))~f.args;
                    f.name = name~"."~f.origname;
                    f.isMethod = true;

                    if(isImported) {
                        f.isExtern = true;
                    }

                    MethodTable[cast(immutable)[name,f.origname]] = f;
                    
                    methods ~= f;
                }
            }
        }
        /*if(name == "va_list") {
            LLVMTypeRef[] t1 = [LLVMPointerType(LLVMInt8TypeInContext(Generator.Context),0)];
            LLVMTypeRef ft1 = LLVMFunctionType(LLVMVoidTypeInContext(Generator.Context),t1.ptr,1,false);
            Generator.LLVMFunctions["va_start"] = LLVMAddFunction(Generator.Module,toStringz("llvm.va_start"),ft1);
            Generator.LLVMFunctions["va_end"] = LLVMAddFunction(Generator.Module,toStringz("llvm.va_end"),ft1);
            Generator.LLVMFunctionTypes["va_start"] = LLVMVoidTypeInContext(Generator.Context);
            Generator.LLVMFunctionTypes["va_end"] = LLVMVoidTypeInContext(Generator.Context);
        }*/
        return values.dup;
    }

    override LLVMValueRef generate() {
        Generator.Structs[name] = LLVMStructCreateNamed(Generator.Context,toStringz(name));
        LLVMTypeRef[] params = getParameters();
        LLVMStructSetBody(Generator.Structs[name],params.ptr,cast(uint)params.length,false);
        if(_this !is null) _this.generate();
        for(int i=0; i<methods.length; i++) {
            methods[i].check();
            methods[i].generate();
        }
        return null;
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
                        Generator.GenerateType(type),
                        toStringz("ifcast")
                    );
                }
                return LLVMBuildIntCast(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(type),
                    toStringz("iicast")
                );
            }

            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMPointerTypeKind) {
                return LLVMBuildPtrToInt(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(b),
                    toStringz("picast")
                );
            }

            if(b.isFloat()) return LLVMBuildFPCast(
                Generator.Builder,
                gval,
                Generator.GenerateType(type),
                toStringz("ffcast")
            );
            return LLVMBuildFPToSI(
                Generator.Builder,
                gval,
                Generator.GenerateType(type),
                toStringz("ficast")
            );
        }
        if(TypePointer p = type.instanceof!TypePointer) {
            if(LLVMGetTypeKind(LLVMTypeOf(gval)) == LLVMIntegerTypeKind) {
                return LLVMBuildIntToPtr(
                    Generator.Builder,
                    gval,
                    Generator.GenerateType(p),
                    toStringz("ipcast")
                );
            }
            return LLVMBuildPointerCast(
                Generator.Builder,
                gval,
                Generator.GenerateType(type),
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
        // TODO
        foreach(f; FuncTable) {
            if(f.namespacesNames.length>0) {
                string[] newNamespacesNames;
                for(int i=0; i<f.namespacesNames.length; i++) {
                    if(f.namespacesNames[i] != namespace) newNamespacesNames ~= f.namespacesNames[i];
                }
                f.namespacesNames = newNamespacesNames.dup;
                string oldname = f.name;
                f.name = namespacesNamesToString(f.namespacesNames, f.origname);
                FuncTable[f.name] = f;
                //FuncTable.remove(oldname);
                Generator.Functions[f.name] = Generator.Functions[oldname];
                //Generator.Functions.remove(oldname);
            }
        }
        foreach(var; MacroTable) {
            if(var.namespacesNames.length>0) {
                string[] newNamespacesNames;
                for(int i=0; i<var.namespacesNames.length; i++) {
                    if(var.namespacesNames[i] != namespace) newNamespacesNames ~= var.namespacesNames[i];
                }
                var.namespacesNames = newNamespacesNames.dup;
                string oldname = var.name;
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
                Generator.Structs[var.name] = Generator.Structs[oldname];
                if(oldname.into(FuncTable)) {
                    FuncTable[var.name] = FuncTable[oldname];
                    Generator.Functions[var.name] = Generator.Functions[oldname];
                }
                foreach(s; StructTable[oldname].elements) {
                    if(NodeFunc f = s.instanceof!NodeFunc) {
                        if(f.origname != "this") MethodTable[cast(immutable)[var.name,f.origname]] = MethodTable[cast(immutable)[oldname,f.origname]];
                        else {
                            FuncTable[var.name] = FuncTable[oldname];
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
            g.isEqu = true;
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