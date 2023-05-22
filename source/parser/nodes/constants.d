module parser.nodes.constants;

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
import parser.ast : Node, into, checkError, countOf, Generator;

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
    double value;
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
    string value;
    bool isWide = false;

    this(string value, bool isWide) {
        this.value = value;
        this.isWide = isWide;
    }

    override Node copy() {
        return new NodeString(value, isWide);
    }

    override Type getType() {
        return new TypePointer(new TypeBasic((isWide ? BasicType.Uint : BasicType.Char)));
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
    char value;
    bool isWide = false;

    this(char value, bool isWide = false) {
        this.value = value;
        this.isWide = isWide;
    }

    override Node copy() {
        return new NodeChar(value, isWide);
    }

    override Type getType() {
        return new TypeBasic((isWide ? BasicType.Uint : BasicType.Char));
    }

    override LLVMValueRef generate() {
        if(!isWide) return LLVMConstInt(LLVMInt8TypeInContext(Generator.Context),to!ulong(value),false);
        return LLVMConstInt(LLVMInt32TypeInContext(Generator.Context),to!ulong(value),false);
    }

    override void debugPrint() {
        writeln("NodeChar('"~to!string(value)~"')");
    }
}