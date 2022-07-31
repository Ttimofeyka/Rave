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
NodeVar[string[]] VarTable;
NodeStruct[string] StructTable;
NodeMacro[string] MacroTable;
bool[string] ConstVars;

bool into(T, TT)(T t, TT tt) {
    pragma(inline,true);
    return !(t !in tt);
}

void checkError(int loc,string msg) {
        writeln("\033[0;31mError on "~to!string(loc+1)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
}

extern(C) void LLVMSetGlobalDSOLocal(LLVMValueRef global);

class LLVMGen {
    LLVMModuleRef Module;
    LLVMContextRef Context;
    LLVMBuilderRef Builder;
    LLVMValueRef[string] Globals;
    LLVMValueRef[string] Functions;
    LLVMTypeRef[string] Structs;
    LLVMBasicBlockRef currBB;

    this() {
        Context = LLVMContextCreate();
        Module = LLVMModuleCreateWithNameInContext(toStringz("rave"),Context);
        LLVMInitializeAllTargets();
        LLVMInitializeAllAsmParsers();
        LLVMInitializeAllAsmPrinters();
        LLVMInitializeAllTargetInfos();
        LLVMInitializeAllTargetMCs();
    }

    string mangle(string name, bool isFunc) {
        pragma(inline,true);
        return isFunc ? "_Ravef"~to!string(name.length)~name : "_Raveg"~name;
    }

    void error(int loc,string msg) {
        writeln("\033[0;31mError on "~to!string(loc+1)~" line: " ~ msg ~ "\033[0;0m");
		exit(1);
    }

    LLVMTypeRef GenerateType(Type t) {
        if(t.instanceof!TypeAlias) return null;
        if(t.instanceof!TypeBasic) {
            TypeBasic b = cast(TypeBasic)t;
            switch(b.type) {
                case BasicType.Bool: return LLVMInt1TypeInContext(Generator.Context);
                case BasicType.Char: return LLVMInt8TypeInContext(Generator.Context);
                case BasicType.Short: return LLVMInt16TypeInContext(Generator.Context);
                case BasicType.Int: return LLVMInt32TypeInContext(Generator.Context);
                case BasicType.Long: return LLVMInt64TypeInContext(Generator.Context);
                case BasicType.Cent: return LLVMInt128TypeInContext(Generator.Context);
                case BasicType.Float: return LLVMFloatTypeInContext(Generator.Context);
                case BasicType.Double: return LLVMDoubleTypeInContext(Generator.Context);
                default: assert(0);
            }
        }
        if(TypePointer p = t.instanceof!TypePointer) {
            LLVMTypeRef a = LLVMPointerType(this.GenerateType(p.instance),0);
            return a;
        }
        if(TypeArray a = t.instanceof!TypeArray) {
            return LLVMArrayType(this.GenerateType(a.element),cast(uint)a.count);
        }
        if(TypeStruct s = t.instanceof!TypeStruct) {
            return Generator.Structs[s.name];
        }
        if(t.instanceof!TypeVoid) return LLVMVoidTypeInContext(Generator.Context);
        if(TypeFunc f = t.instanceof!TypeFunc) {
            LLVMTypeRef[] types;
            foreach(ty; f.args) {
                types ~= Generator.GenerateType(ty);
            }
            return LLVMFunctionType(
                Generator.GenerateType(f.main),
                types.ptr,
                cast(uint)types.length,
                false
            );
        }
        assert(0);
    }

    uint getAlignmentOfType(Type t) {
        if(TypeBasic bt = t.instanceof!TypeBasic) {
            switch(bt.type) {
                case BasicType.Bool:
                case BasicType.Char:
                    return 1;
                case BasicType.Short:
                    return 2;
                case BasicType.Int:
                case BasicType.Float:
                    return 4;
                case BasicType.Long:
                case BasicType.Double:
                    return 8;
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
        assert(0);
    }
}

const(string[]) cnv(string[] s) {return cast(const)s;}

void clearLocals() {
    foreach(key; byKey(VarTable)) {
        if(key[0] == "local") VarTable.remove(key);
    }
}

NodeVar[] getLocals() {
    NodeVar[] locals;
    foreach(key; byKey(VarTable)) {
        if(key[0] == "local") locals ~= VarTable[key];
    }
    return locals.dup;
}

class Scope {
    LLVMValueRef[string] localscope;
    int[string] args;
    string func;
    LLVMBasicBlockRef BlockExit;
    bool funcHasRet = false;
    Node[int] macroArgs;

    this(string func, int[string] args) {
        this.func = func;
        this.args = args.dup;
    }

    LLVMValueRef getWithoutLoad(string n) {
        if(!(n !in localscope)) return localscope[n];
        if(!(n !in Generator.Globals)) return Generator.Globals[n];
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
            LLVMSetAlignment(instr,Generator.getAlignmentOfType(VarTable[["local",n].cnv].t));
            return instr;
        }
        if(!(n !in Generator.Globals)) {
            auto instr = LLVMBuildLoad(
                Generator.Builder,
                Generator.Globals[n],
                toStringz("loadGlobal"~n)
            );
            LLVMSetAlignment(instr,Generator.getAlignmentOfType(VarTable[["global",n].cnv].t));
            return instr;
        }
        return LLVMGetParam(
            Generator.Functions[func],
            cast(uint)args[n]
        );
    }

    NodeVar getVar(string n) {
        writeln(n);
        foreach(key; byKey(VarTable)) {writeln(key);}
        if(n.into(localscope)) return VarTable[["local",n].cnv];
        if(n.into(Generator.Globals)) return VarTable[["global",n].cnv];
        Generator.error(-1,"Undefined variable \""~n~"\"!");
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
    }

    void debugPrint() {
        writeln("Node");
    }

    void check() {}
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
    private double value;

    this(double value) {
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
        return LLVMConstInt(LLVMInt8Type(),to!ulong(value),false);
    }

    override void debugPrint() {
        writeln("NodeChar('"~to!string(value)~"')");
    }
}

class NodeBinary : Node {
    TokType operator;
    Node first;
    Node second;
    int loc;

    this(TokType operator, Node first, Node second, int line) {
        this.first = first;
        this.second = second;
        this.operator = operator;
        this.loc = line;
    }

    LLVMValueRef sum(LLVMValueRef one, LLVMValueRef two) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
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
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            Generator.error(loc,"value types are incompatible!");
        }
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
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            Generator.error(loc,"value types are incompatible!");
        }
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
        }
        Generator.error(loc,"Comparing values not with the same types!");
        assert(0);
    }

    LLVMValueRef diff(LLVMValueRef one, LLVMValueRef two, bool less) {
        if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
            if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMIntegerTypeKind) {
                return LLVMBuildICmp(
                    Generator.Builder,
                    (less ? LLVMIntSLT : LLVMIntSGT),
                    one,
                    two,
                    toStringz("icmp")
                );
            }
            else if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMFloatTypeKind) {
                return LLVMBuildFCmp(
                    Generator.Builder,
                    (less ? LLVMRealOLT : LLVMRealOGT),
                    one,
                    two,
                    toStringz("fcmp")
                );
            }
        }
        Generator.error(loc,"Comparing values not with the same types!");
        assert(0);
    }

    override LLVMValueRef generate() {
        if(operator == TokType.Plus) return sum(first.generate(),second.generate());
        if(operator == TokType.Minus) return sub(first.generate(),second.generate());
        if(operator == TokType.Multiply) return mul(first.generate(),second.generate());
        if(operator == TokType.Divide) return div(first.generate(),second.generate());
        if(operator == TokType.Equ) {
            if(NodeIden i = first.instanceof!NodeIden) {
                if(i.name.into(ConstVars)) {
                    Generator.error(loc, "An attempt to change the value of a constant variable!");
                }
                if(i.name.into(AliasTable)) {
                    AliasTable[i.name] = second;
                    return null;
                }
                return LLVMBuildStore(
                    Generator.Builder,
                    second.generate(),
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
        }
        if(operator == TokType.Equal) return equal(first.generate(),second.generate(),false);
        if(operator == TokType.Nequal) return equal(first.generate(),second.generate(),true);
        if(operator == TokType.Less) return diff(first.generate(),second.generate(),true);
        if(operator == TokType.More) return diff(first.generate(),second.generate(),false);
        return null;
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
                    else LLVMBuildStore(
                        Generator.Builder,
                        StructTable[s.name].predefines[i].value.generate(),
                        LLVMBuildStructGEP(
                            Generator.Builder,
                            var,
                            cast(uint)i,
                            toStringz("getStructElement")
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
        if(!([(isGlobal ? "global" : "local"),name].cnv !in VarTable)) {
            checkError(loc,"a variable with that name already exists on "~to!string(VarTable[[(isGlobal ? "global" : "local"),name].cnv].loc+1)~" line!");
        }
        VarTable[[(isGlobal ? "global" : "local"),name].cnv] = this;
        if(isConst && isGlobal) ConstVars[name] = true;
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
                toStringz((noMangling) ? name : Generator.mangle(name,false))
            );
            if(isExtern) {
                LLVMSetLinkage(Generator.Globals[name], LLVMExternalLinkage);
            }
            LLVMSetAlignment(Generator.Globals[name],Generator.getAlignmentOfType(t));
            LLVMSetGlobalDSOLocal(Generator.Globals[name]);
            if(value !is null && !isExtern) LLVMSetInitializer(Generator.Globals[name], value.generate());
        }
        else {
            // Local variables
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
        if(!currScope.has(name)) {
            Generator.error(loc,"unknown identifier \""~name~"\"!");
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
            checkError(loc,"a function with that name already exists on "~to!string(FuncTable[name].loc+1)~" line!");
        }
        FuncTable[name] = this;
        for(int i=0; i<block.nodes.length; i++) {
            block.nodes[i].check();
        }
    }

    LLVMTypeRef* getParameters() {
        LLVMTypeRef[] p;
        for(int i=0; i<args.length; i++) {
            p ~= Generator.GenerateType(args[i].type);
        }
        return p.ptr;
    }

    override LLVMValueRef generate() {
        bool noMangling = false;
        bool vararg = false;

        for(int i=0; i<mods.length; i++) {
            if(mods[i].name == "C") noMangling = true;
            else if(mods[i].name == "vararg") vararg = true;
        }

        LLVMTypeRef functype = LLVMFunctionType(
            Generator.GenerateType(type),
            getParameters(),
            cast(uint)args.length,
            vararg
        );

        Generator.Functions[name] = LLVMAddFunction(
            Generator.Module,
            toStringz((noMangling) ? name : Generator.mangle(name,true)),
            functype
        );

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
            for(int i=0; i<args.length; i++) {
                intargs[args[i].name] = i;
            }

            currScope = new Scope(name,intargs);
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

				if(retBlocks.length == 1 || retBlocks[0] == null) {
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
            clearLocals();
        }

        LLVMSetGlobalDSOLocal(Generator.Functions[name]);

        //writeln(fromStringz(LLVMPrintModuleToString(Generator.Module)));

        LLVMVerifyFunction(Generator.Functions[name],0);

        return null;
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

    LLVMValueRef* getParameters() {
        LLVMValueRef[] params;
        for(int i=0; i<args.length; i++) {
            params ~= args[i].generate();
        }
        return params.ptr;
    }

    override LLVMValueRef generate() {
        NodeIden f = func.instanceof!NodeIden;
        if(!f.name.into(FuncTable)) {
            // Generate macro-call
            Scope oldScope = currScope;
            currScope = new Scope(f.name, null);
            for(int i=0; i<args.length; i++) {
                currScope.macroArgs[i] = args[i];
                //currScope.localscope[MacroTable[f.name].args[i]] = args[i].generate();
            }
            MacroTable[f.name].block.generate();
            
            LLVMValueRef toret = null;
            
            if(MacroTable[f.name].ret !is null) {
                MacroTable[f.name].ret.parent = f.name;
                toret = MacroTable[f.name].ret.generate();
            }
            currScope = oldScope;
            return toret;
        }
        string name = "CallFunc"~f.name;
        if(FuncTable[f.name].type.instanceof!TypeVoid) name = "";
        return LLVMBuildCall(
            Generator.Builder,
            Generator.Functions[f.name],
            getParameters(),
            cast(uint)args.length,
            toStringz(name)
        );
    }

    override void debugPrint() {
        writeln("NodeCall()");
    }
}

class NodeIndex : Node {
    Node element;
    int index;
    int loc;
    bool isEqu = false;

    this(Node element, int index, int loc) {
        this.element = element;
        this.index = index;
        this.loc = loc;
    }

    override void check() {element.check();}
    override LLVMValueRef generate() {
        if(!isEqu) {
            
        }
        return null;
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

    TypeStruct getStruct() {
        if(NodeIden id = base.instanceof!NodeIden) {
            return currScope.getVar(id.name).t.instanceof!TypeStruct;
        }
        else {
            NodeGet g = base.instanceof!NodeGet;
            TypeStruct prestruct = g.getStruct();

            StructMember member = structsNumbers[cast(immutable)[prestruct.name,g.field]];
            NodeVar parentvar = member.var;
            return parentvar.t.instanceof!TypeStruct;
        }
    }

    LLVMValueRef generateForParent() {
        if(NodeIden id = base.instanceof!NodeIden) {
            TypeStruct s = currScope.getVar(id.name).t.instanceof!TypeStruct;
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
    }

    override LLVMValueRef generate() {
        if(NodeIden id = base.instanceof!NodeIden) {
            TypeStruct s = currScope.getVar(id.name).t.instanceof!TypeStruct;
            if(!isEqu) return LLVMBuildLoad(
                Generator.Builder,
                LLVMBuildStructGEP(
                    Generator.Builder,
                    currScope.getWithoutLoad(id.name),
                    structsNumbers[cast(immutable)[s.name,field]].number,
                    toStringz("getStructElement"~field)
                ),
                toStringz("loadGEP")
            );
            return LLVMBuildStructGEP(
                    Generator.Builder,
                    currScope.getWithoutLoad(id.name),
                    structsNumbers[cast(immutable)[s.name,field]].number,
                    toStringz("getStructElement"~field)
            );
        }
        else if(NodeGet g = base.instanceof!NodeGet) {
            g.isEqu = true;

            TypeStruct s = getStruct();
            
            if(isEqu) return LLVMBuildStructGEP(
                Generator.Builder,
                g.generate(),
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
        exit(1);
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
        return LLVMConstInt(LLVMInt1Type(),to!ulong(value),false);
    }
}

class NodeUnary : Node {
    int loc;
    TokType type;
    Node base;

    this(int loc, TokType type, Node base) {
        this.loc = loc;
        this.type = type;
        this.base = base;
    }

    override void check() {base.check();}
    override LLVMValueRef generate() {
        writeln(type);
        if(type == TokType.Minus) return LLVMBuildNeg(
            Generator.Builder,
            base.generate(),
            toStringz("neg")
        );
        assert(0);
    }
}

class NodeIf : Node {
    Node condition;
    Node _body;
    Node _else;
    int loc;
    string func;

    this(Node condition, Node _body, Node _else, int loc, string func) {
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
            }
            else if(NodeBlock b = _body.instanceof!NodeBlock) {
                foreach(n; b.nodes) {
                    if(NodeRet r = n.instanceof!NodeRet) r.parent = func;
                }
            }
        }

        if(_else !is null) {
            if(NodeRet r = _else.instanceof!NodeRet) {
                r.parent = func;
            }
            else if(NodeBlock b = _else.instanceof!NodeBlock) {
                foreach(n; b.nodes) {
                    if(NodeRet r = n.instanceof!NodeRet) r.parent = func;
                }
            }
        }

        condition.check();
        if(_body !is null) _body.check();
        if(_else !is null) _else.check();
    }

    override LLVMValueRef generate() {
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

        LLVMPositionBuilderAtEnd(Generator.Builder, thenBlock);
        Generator.currBB = thenBlock;
        if(_body !is null) _body.generate();

        LLVMPositionBuilderAtEnd(Generator.Builder, elseBlock);
		Generator.currBB = elseBlock;
        if(_else !is null) _else.generate();
        else LLVMBuildBr(Generator.Builder,endBlock);

        LLVMPositionBuilderAtEnd(Generator.Builder, endBlock);
        Generator.currBB = endBlock;

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
        LLVMBasicBlockRef whileBlock = 
            LLVMAppendBasicBlock(
                Generator.Functions[currScope.func],
                toStringz("while")
        );

        currScope.BlockExit = LLVMAppendBasicBlock(
            Generator.Functions[currScope.func],
            toStringz("exit")
        );

        LLVMBuildCondBr(
            Generator.Builder,
            condition.generate(),
            whileBlock,
            currScope.BlockExit
        );

        LLVMPositionBuilderAtEnd(Generator.Builder, whileBlock);
        _body.generate();

        LLVMBuildCondBr(
            Generator.Builder,
            condition.generate(),
            whileBlock,
            currScope.BlockExit
        );

        LLVMPositionBuilderAtEnd(Generator.Builder, currScope.BlockExit);

        return null;
    }
}

class NodeBreak : Node {
    int loc;

    this(int loc) {this.loc = loc;}

    override void check() {}
    override LLVMValueRef generate() {
        LLVMBuildBr(Generator.Builder,currScope.BlockExit);
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
                if(!v.isExtern) v.isExtern = isImport;
                v.generate();
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

    this(string name, Node[] elements, int loc) {
        this.name = name;
        this.elements = elements.dup;
        this.loc = loc;
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
                values ~= Generator.GenerateType(v.t);
            }
            else {
                NodeFunc f = elements[i].instanceof!NodeFunc;
            }
        }
        return values.dup;
    }

    override LLVMValueRef generate() {
        Generator.Structs[name] = LLVMStructCreateNamed(Generator.Context,toStringz(name));
        LLVMTypeRef[] params = getParameters();
        LLVMStructSetBody(Generator.Structs[name],params.ptr,cast(uint)params.length,false);
        StructTable[name] = this;
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

    this(NodeBlock block, string name, string[] args, int loc) {
        this.block = block;
        this.name = name;
        this.args = args;
        this.loc = loc;
    }

    override void check() {
        if(namespacesNames.length>0) {
            name = namespacesNamesToString(namespacesNames,name);
        }
        if(name.into(MacroTable)) {
            checkError(loc,"a macro with that name already exists on "~to!string(MacroTable[name].loc+1)~" line!");
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

    override void check() {
        foreach(v; byValue(VarTable)) {
            if(v.namespacesNames.canFind(namespace)) {
                string[] namespaces;
                for(int i=0; i<v.namespacesNames.length; i++) {
                    if(v.namespacesNames[i] != namespace) namespaces ~= v.namespacesNames[i];
                }
                string oldname = v.name;
                v.name = namespacesNamesToString(v.namespacesNames,v.origname);
                VarTable.remove([(v.isGlobal ? "global" : "local"),oldname].cnv);
                VarTable[[(v.isGlobal ? "global" : "local"),oldname].cnv] = v;
            }
        }
        foreach(v; byValue(FuncTable)) {
            if(v.namespacesNames.canFind(namespace)) {
                string[] namespaces;
                for(int i=0; i<v.namespacesNames.length; i++) {
                    if(v.namespacesNames[i] != namespace) namespaces ~= v.namespacesNames[i];
                }
                v.namespacesNames = namespaces.dup;
                string oldname = v.name;
                v.name = namespacesNamesToString(v.namespacesNames,v.origname);
                FuncTable.remove(oldname);
                FuncTable[oldname] = v;
            }
        }
    }

    override LLVMValueRef generate() {return null;}
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