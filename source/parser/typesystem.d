module parser.typesystem;

import parser.util;

import std.conv;
import std.stdio;
import std.array;

class Type {
    bool assignable(Type other) { return false; }

    override string toString() const {
        return "?";
    }
}

class TypeVoid : Type {
    override bool assignable(Type other) {
        return other.instanceof!TypeVoid() !is null;
    }

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
    t_bool
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
    case BasicType.t_bool   : return 1;
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
            if(basic == BasicType.t_bool) return true;
            else return basicTypeSizeOf(basic) >= basicTypeSizeOf(other2.basic);
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
        if(to is null) {return "";}
        return to.toString() ~ "*";
    }
}

class TypeStruct : Type {
    Type[string] fields;
    TypeFunction[string] methods;

    this(Type[string] fields, TypeFunction[string] methods) {
        this.fields = fields;
        this.methods = methods;
    }

    override bool assignable(Type other) {
        // if(other.instanceof!(TypeStruct)) return true;
        return this == other;
    }

    override string toString() const {
        string s = "struct {";

        if(fields.length > 0) {
            auto vals = array(fields.byKeyValue);
            s ~= " " ~ vals[0].key ~ ": " ~ vals[0].value.toString();
            for(int i = 1; i < vals.length; ++i) {
                s ~= ", " ~ vals[i].key ~ ": " ~ vals[i].value.toString();
            }
        }
        s ~= " }";
        return s;
    }
}

class TypeType : Type {
    Type type;

    this(Type type) { this.type = type; }

    override string toString() const {
        return "type " ~ type.toString();
    }
}

class TypeFunctionLike : Type {
    override bool assignable(Type other) {
        return other.instanceof!TypeFunction !is null;
    }

    override string toString() const {
        return "a callable";
    }
}

class TypeFunction : Type {
	Type ret;
    Type[] args;

    this(Type ret, Type[] args) {
        this.ret = ret;
        this.args = args;
    }

    override bool assignable(Type other) {
        // if(other.instanceof!(TypeFunction)) return true;
        return false;
    }

    override string toString() const {
        string s = "";

        s ~= "(";

        if(args.length > 0) {
            s ~= args[0].toString();
            for(int i = 1; i < args.length; ++i) {
                s ~= ", " ~ args[i].toString();
            }
        }
        s ~= "): " ~ ret.toString();
        return s;
    }

    string toString(string name, string[] argNames) const {
        string s = "";

        s ~= name ~ "(";

        if(args.length > 0) {
            s ~= argNames[0] ~ ": " ~ args[0].toString();
            for(int i = 1; i < args.length; ++i) {
                s ~= ", " ~ argNames[i] ~ ": " ~ args[i].toString();
            }
        }
        s ~= "): " ~ ret.toString();
        return s;
    }
}
