// Licensed by LGPL-3.0-or-later

module parser.types;

import std.string;
import std.array;
import std.conv : to;
import std.ascii : isAlpha;
import parser.ast : Node;
import std.stdio;
import parser.parser : instanceof;

enum BasicType {
    Bool,
    //Byte,
    Char,
    Short,
    Int,
    Long,
    Cent,
    Float,
    Double,

    Ushort,
    Uint,
    Ulong,
    Uchar
}



Type getType(string name) {
    switch(name) {
        case "bool": return new TypeBasic("bool");
        case "char": return new TypeBasic("char");
        case "int": return new TypeBasic("int");
        case "short": return new TypeBasic("short");
        case "long": return new TypeBasic("long");
        case "float": return new TypeBasic("float");
        case "double": return new TypeBasic("double");
        case "cent": return new TypeBasic("cent");

        case "ubool": return new TypeBasic("ubool");
        case "uint": return new TypeBasic("uint");
        case "ushort": return new TypeBasic("ushort");
        case "ulong": return new TypeBasic("ulong");
        case "uchar": return new TypeBasic("uchar");

        case "alias": return new TypeAlias();
        default: return new TypeStruct(name);
    }
}

class Type {
    Type copy() {assert(0);}

    override string toString()
    {
        return "SimpleType";
    }
}

class TypeBasic : Type {
    BasicType type;
    private string value;

    this(string value) {
        this.value = value;
        switch(value) {
            case "int":
                type = BasicType.Int; break;
            case "bool":
                type = BasicType.Bool; break;
            case "short":
                type = BasicType.Short; break;
            case "long":
                type = BasicType.Long; break;
            case "float":
                type = BasicType.Float; break;
            case "double":
                type = BasicType.Double; break;
            case "char":
                type = BasicType.Char; break;
            case "cent":
                type = BasicType.Cent; break;

            case "uint":
                type = BasicType.Uint; break;
            case "ushort":
                type = BasicType.Ushort; break;
            case "ulong":
                type = BasicType.Ulong; break;
            case "uchar":
                type = BasicType.Uchar; break;
            default: break;
        }
    }
    
    override Type copy() {
        return new TypeBasic(this.value);
    }

    bool isFloat() {
        pragma(inline,true);
        return (type == BasicType.Float || type == BasicType.Double);
    }

    override string toString()
    {
        return this.value;
    }
}

class TypePointer : Type {
    Type instance;

    this(Type instance) {this.instance = instance;}

    override Type copy() {
        return new TypePointer(instance.copy());
    }

    override string toString()
    {
        return instance.toString()~"*";
    }
}

class TypeArray : Type {
    int count;
    Type element;

    this(int count, Type element) {
        this.count = count;
        this.element = element;
    }

    override Type copy() {
        return new TypeArray(this.count,this.element.copy());
    }

    override string toString()
    {
        return element.toString()~"["~to!string(count)~"]";
    }
}

class TypeAlias : Type {
    override Type copy() {
        return new TypeAlias();
    }
}
class TypeVoid : Type {
    override Type copy() {
        return new TypeVoid();
    }

    override string toString()
    {
        return "void";
    }
}
class TypeStruct : Type {
    string name;
    Type[] types;

    this(string name) {this.name = name;}
    this(string name, Type[] types) {this.name = name; this.types = types.dup;}

    override Type copy() {
        Type[] _types;
        for(int i=0; i<types.length; i++) {
            _types ~= types[i].copy();
        }
        return new TypeStruct(name,_types);
    }

    void updateByTypes() {
        this.name = this.name[0..this.name.indexOf('<')]~"<";
        for(int i=0; i<types.length; i++) {
            this.name ~= types[i].toString()~",";
        }
        this.name = this.name[0..$-1]~">";
    }

    override string toString()
    {
        return name;
    }
}
class TypeFuncArg : Type {
    Type type;
    string name;

    override Type copy() {
        return new TypeFuncArg(type.copy(),name);
    }

    this(Type type, string name) {
        this.type = type;
        this.name = name;
    }
}
class TypeFunc : Type {
    Type main;
    TypeFuncArg[] args;

    override Type copy() {
        TypeFuncArg[] _copied;

        for(int i=0; i<args.length; i++) {
            _copied ~= args[i].copy().instanceof!TypeFuncArg;
        }

        return new TypeFunc(main.copy(),_copied.dup);
    }

    this(Type main, TypeFuncArg[] args) {
        this.main = main;
        this.args = args;
    }

    override string toString()
    {
        return "NotImplemented";
    }
}
class TypeVarArg : Type {}
class TypeMacroArg : Type { int num; this(int num) {this.num = num;} }
class TypeBuiltin : Type { string name; Node[] args; this(string name, Node[] args) {this.name = name; this.args = args.dup;} }
class TypeCall : Type {
    string name;
    Node[] args;

    this(string name, Node[] args) {
        this.name = name;
        this.args = args.dup;
    }

    override string toString() {
        return "FuncCall";
    }
}
class TypeAuto : Type {
    override Type copy() {
        return new TypeAuto();
    }

    this() {}
}
class TypeConst : Type {
    Type instance;

    this(Type instance) {
        this.instance = instance;
    }

    override Type copy() {
        return new TypeConst(this.instance.copy());
    }

    override string toString() {return "const("~instance.toString()~")";}
}