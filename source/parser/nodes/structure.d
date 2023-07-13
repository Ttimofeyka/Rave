module parser.nodes.structure;

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
import parser.ast, parser.nodes.constants;

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
                Generator.GenerateType(_ts,loc);
            }
            Generator.toReplace[templateNames[i]] = _types[i];
            _fn ~= templateNames[i]~",";
        }

        Generator.toReplace[name~_fn[0..$-1]~">"] = new TypeStruct(name~typesS);

        NodeStruct _struct = new NodeStruct(name~typesS,copyElements().dup,loc,"",[],mods.dup);
        _struct.isTemplated = true;
        _struct.isComdat = true;

        _struct.check();
        for(int i=0; i<_struct.elements.length; i+=1) {
            if(NodeVar nv = _struct.elements[i].instanceof!NodeVar) {
                Type[] types = [nv.t.copy()];
                int typesIdx = 0;
                bool needToReplace = false;
                while(true) {
                    Type ty = types[typesIdx];
                    if(ty.instanceof!TypeBasic || ty.instanceof!TypeFunc) break;
                    else if(TypePointer tp = ty.instanceof!TypePointer) {
                        types ~= tp.instance;
                        typesIdx += 1;
                    }
                    else if(TypeArray ta = ty.instanceof!TypeArray) {
                        types ~= ta.element;
                        typesIdx += 1;
                    }
                    else if(TypeStruct ts = ty.instanceof!TypeStruct) {
                        if(ts.name.into(Generator.toReplace)) {
                            needToReplace = true;
                            types ~= Generator.toReplace[ts.name];
                            typesIdx += 1;
                        }
                        break;
                    }
                    else break;
                }
                if(needToReplace) {
                    Type doneType = types[typesIdx];
                    if(typesIdx == 0) nv.t = doneType.copy();
                    else {
                        while(true) {
                            typesIdx -= 1;
                            if(typesIdx < 0) break;
                            if(TypePointer tp = types[typesIdx].instanceof!TypePointer) {
                                tp.instance = doneType.copy();
                                doneType = tp.copy();
                            }
                            else if(TypeArray ta = types[typesIdx].instanceof!TypeArray) {
                                ta.element = doneType.copy();
                                doneType = ta.copy();
                            }
                            else if(TypeStruct ts = types[typesIdx].instanceof!TypeStruct) {
                                if(ts.name.indexOf("<") != -1) {
                                    // Template
                                    ts.updateByTypes();
                                    doneType = ts.copy();
                                }
                            }
                        }
                        nv.t = doneType.copy();
                    }
                }
                _struct.elements[i] = nv;
            }
        }
        _struct.generate();

        Generator.ActiveLoops = activeLoops;
        Generator.Builder = builder;
        Generator.currBB = currBB;
        currScope = _scope;
        Generator.toReplace = toReplace.dup;

        return Generator.Structs[_struct.name];
    }
}