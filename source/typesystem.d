module typesystem;

class Type {
    bool assignable(Type other) { return false; }
}

class TypeVoid : Type {
    override bool assignable(Type other) { return false; }
}

class TypeUnknown : Type {
    override bool assignable(Type other) { return false; }
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

class TypeBasic : Type {
    BasicType basic;
    
    this(BasicType basic) {
        this.basic = basic;
    }

    override bool assignable(Type other) { return false; }
}

class TypePointer : Type {
    Type to;
    this(Type to) {
        this.to = to;
    }

    override bool assignable(Type other) {
        return false;
    }
}
