module parser.typesystem;
import parser.util;
import std.conv;

class Type {
    bool assignable(Type other) { return false; }

    override string toString() const {
        return "?";
    }
}

class TypeVoid : Type {
    override bool assignable(Type other) { return false; }

    override string toString() const {
        return "void";
    }
}

class TypeUnknown : Type {
    override bool assignable(Type other) { return false; }

    override string toString() const {
        return "?unknown";
    }
}

class TypeConst : Type {
    Type base;

    this(Type base) {
        this.base = base;
    }

    override bool assignable(Type other) { return false; }

    override string toString() const {
        return "const " ~ base.toString();
    }
}

enum BasicType {
    t_int,
    t_short,
    t_long,
    t_size,
    t_char,
    t_uint,
    t_ushort,
    t_ulong,
    t_usize,
    t_uchar,
    t_float,
}

size_t basicTypeSizeOf(BasicType t) {
    // TODO: Change based on the target platform!
    switch(t) {
    case BasicType.t_int    : return 32 / 8;
    case BasicType.t_short  : return 16 / 8;
    case BasicType.t_long   : return 64 / 8;
    case BasicType.t_size   : return 64 / 8;
    case BasicType.t_char   : return 8 / 8;
    case BasicType.t_uint   : return 32 / 8;
    case BasicType.t_ushort : return 16 / 8;
    case BasicType.t_ulong  : return 64 / 8;
    case BasicType.t_usize  : return 64 / 8;
    case BasicType.t_uchar  : return 8 / 8;
    case BasicType.t_float  : return 32 / 8;
    default: return 0;
    }
}

class TypeBasic : Type {
    BasicType basic;
    
    this(BasicType basic) {
        this.basic = basic;
    }

    override string toString() const {
        return to!string(basic)[2..$];
    }

    override bool assignable(Type other) {
        if(auto other2 = other.instanceof!(TypeBasic)) {
            return basicTypeSizeOf(basic) >= basicTypeSizeOf(other2.basic);
        }
        return false;
    }
}

class TypePointer : Type {
    Type to;
    this(Type to) {
        this.to = to;
    }

    override bool assignable(Type other) {
        if(other.instanceof!(TypePointer)) return true;
        return false;
    }

    override string toString() const {
        return to.toString() ~ "*";
    }
}

class SignatureTypes {
    bool isStatic, isExtern;
}

class FunctionSignatureTypes : SignatureTypes {
	Type ret;
    Type[] args;
    this(Type ret, Type[] args) {
        this.ret = ret;
        this.args = args;
    }
}

class VariableSignatureTypes : SignatureTypes {
	Type type;

    this(Type type) { this.type = type; }
}
