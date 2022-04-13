module compiler.compiler;

import parser.typesystem;
import parser.generator.gen;
import parser.analyzer;
import parser.ast;
import parser.atst;

import std.format;
import std.stdio;

struct CompilerDebugInfo {
    bool printAst;
    bool debugMode;
}

struct CompilerError {
    int hadError;
    string reason;
}

class CompilerProgram {
    Declaration[] decls;
    AstNode[] nodes;
}

class Compiler {
    CompilerDebugInfo debugInfo;
    int optLevel;

    CompilerProgram _program;

    CompilerProgram getProgram() { return _program; }

    this(Declaration[] decls, AstNode[] nodes, CompilerDebugInfo debugInfo = CompilerDebugInfo(false)) {
        this._program = new CompilerProgram();
        this._program.decls = decls;
        this._program.nodes = nodes;
    }

    private void implementDefaultTypeContext(AtstTypeContext ctx) {
        ctx.setType("int",    new TypeBasic(BasicType.t_int));
        ctx.setType("short",  new TypeBasic(BasicType.t_short));
        ctx.setType("long",   new TypeBasic(BasicType.t_long));
        ctx.setType("size",   new TypeBasic(BasicType.t_size));
        ctx.setType("char",   new TypeBasic(BasicType.t_char));
        ctx.setType("uint",   new TypeBasic(BasicType.t_uint));
        ctx.setType("ushort", new TypeBasic(BasicType.t_ushort));
        ctx.setType("ulong",  new TypeBasic(BasicType.t_ulong));
        ctx.setType("usize",  new TypeBasic(BasicType.t_usize));
        ctx.setType("uchar",  new TypeBasic(BasicType.t_uchar));
        ctx.setType("float",  new TypeBasic(BasicType.t_float));
        ctx.setType("void",   new TypeVoid());
    }

    public CompilerError generate(string t, string outputFile) {
        if(debugInfo.printAst) writeln("------------------ AST -------------------");

        bool hadErrors = false;
        int errorCount = 0;

        auto genctx = new GenerationContext();
        auto semaScope = new AnalyzerScope(genctx.sema, genctx);

        genctx.optLevel = optLevel;

        implementDefaultTypeContext(genctx.sema.typeContext);

        foreach(decl; _program.decls) {
            writeln("Decl: ", decl.name);
            foreach(node; decl.analyze(semaScope)) {
                writeln("DeclFunc:");
                node.debugPrint(0);
                _program.nodes ~= node;
            }
        }

        foreach(node; _program.nodes) {
            node.analyze(semaScope, null);
            if(genctx.sema.errorCount > 0) {
                hadErrors = true;
                errorCount += genctx.sema.errorCount;
                genctx.sema.flushErrors();
                if(debugInfo.printAst) node.debugPrint(0);
                break;
            }
            
            if(debugInfo.printAst) node.debugPrint(0);
        }

        if(hadErrors) return CompilerError(true, format("Semantic analisys failed with %d errors.", errorCount));

        if(debugInfo.printAst) writeln("------------------ Generating -------------------");
        genctx.gen(t, semaScope, _program.nodes, outputFile, debugInfo.debugMode);
        if(genctx.sema.errorCount > 0) {
            genctx.sema.flushErrors();
            return CompilerError(true, format("Code generation failed with %d errors.", errorCount));
        }
        return CompilerError(false, "Compiled sucessfully");
    }
}
