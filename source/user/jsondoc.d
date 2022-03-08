module user.jsondoc;
import parser.ast;
import parser.util;
import std.json, std.stdio, std.file;

class DocGenerator {
    abstract void output(string filename);
    abstract void generate(AstNode ast);
}

class JSONDocGenerator : DocGenerator {
    JSONValue json;

    this() {
        // string[] arr = ["a", "b"];
        json = ["a": "b"];
    }

    override void output(string filename)
    {
        toFile(json.toPrettyString(), filename);
    }

    override void generate(AstNode ast)
    {
        if(ast.instanceof!AstNodeFunction) {
            AstNodeFunction func = cast(AstNodeFunction)ast;
            JSONValue v = [
                "loc": func.where.toString(),
                "str": func.actualDecl.toString(func.decl.name, func.decl.argNames),
                "doc": func.decl.doc,
            ];
            json.object[func.decl.name] = v;
        }
    }

}
