module gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import typesystem, util;
import ast;
import std.conv : to;
import std.string;

class GenerationContext {
    LLVMExecutionEngineRef engine;
    LLVMModuleRef mod;
	AtstTypeContext typecontext;

    this() {
        mod = LLVMModuleCreateWithName(cast(const char*)"epl");
		typecontext = new AtstTypeContext();

		// Initialization
		LLVMInitializeAllTargets();
		LLVMInitializeAllAsmParsers();
		LLVMInitializeAllAsmPrinters();
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllTargetMCs();

		typecontext.types["int"] = new TypeBasic(BasicType.t_int);
		typecontext.types["short"] = new TypeBasic(BasicType.t_short);
		typecontext.types["char"] = new TypeBasic(BasicType.t_char);
		typecontext.types["long"] = new TypeBasic(BasicType.t_long);
    }

	LLVMTypeRef getLLVMType(AtstNode parse_type) {
		if(parse_type.instanceof!(AtstNodeName)) {
			Type ast_type = parse_type.get(typecontext);
			if(ast_type.instanceof!(TypeBasic)) {
				TypeBasic tb = cast(TypeBasic)ast_type;
				switch(tb.basic) {
					case BasicType.t_int:
						return LLVMInt32Type();
					case BasicType.t_short:
						return LLVMInt16Type();
					case BasicType.t_long:
						return LLVMInt64Type();
					case BasicType.t_char:
						return LLVMInt8Type();
					case BasicType.t_float:
						return LLVMFloatType();
					default: return LLVMInt8Type();
				}
			}
		}
		else if(parse_type.instanceof!(AtstNodePointer)) {
			AtstNodePointer p = cast(AtstNodePointer)parse_type;
			return LLVMPointerType(getLLVMType(p.node), 0);
		}
		return LLVMInt8Type();
	}

	void genFunc(AstNode node) {
		LLVMBuilderRef builder = LLVMCreateBuilder();
		AstNodeFunction astf = 
			cast(AstNodeFunction)node;
		LLVMTypeRef* param_types;

		AstNodeBlock f_body = cast(AstNodeBlock)astf.body_;
		AstNodeReturn ret_ast = cast(AstNodeReturn)
			f_body.nodes[0];

		LLVMTypeRef ret_type = LLVMFunctionType(
		    getLLVMType(astf.decl.signature.ret),
		    param_types,
		    cast(uint)astf.decl.signature.args.length,
		    false
	    );

		LLVMValueRef func = LLVMAddFunction(
		    this.mod,
		    cast(const char*)(astf.decl.name~"\0"),
		    ret_type
	    );

		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func,cast(const char*)"entry");
		LLVMPositionBuilderAtEnd(builder, entry);
		AstNodeInt retint = cast(AstNodeInt)ret_ast.value;

	    LLVMValueRef retval = LLVMConstInt(
	    	getLLVMType(astf.decl.signature.ret),
	    	retint.value,
	    	false
	    );

	    LLVMBuildRet(builder, retval);

		LLVMVerifyFunction(func, 0);
	}

	void genGlobalVar(AstNode node) {
		AstNodeDecl iden = cast(AstNodeDecl)node;
		LLVMBuilderRef builder = LLVMCreateBuilder();

		AtstNode type = iden.decl.type;
		
		AstNodeInt ani = cast(AstNodeInt)iden.value;
		LLVMValueRef constval = LLVMConstInt(
			getLLVMType(type),
			ani.value,
			true
		);

		LLVMValueRef var = LLVMAddGlobal(
			this.mod,
			getLLVMType(type),
			toStringz(iden.decl.name)
		);

		LLVMSetInitializer(var, constval); // Check this for errors! (segfault)

		auto a = LLVMValueAsBasicBlock(var);
		LLVMPositionBuilderAtEnd(builder, a);

		LLVMBuildAlloca(builder, getLLVMType(type), toStringz(iden.decl.name));
	}

	void genNode(AstNode node) {
		if(node.instanceof!(AstNodeFunction)) {
			genFunc(node);
		}
		else if(node.instanceof!(AstNodeDecl)) {
			genGlobalVar(node);
		}
	}

    void gen(AstNode[] nodes,string file,bool debugMode) {
		for(int i=0; i<nodes.length; i++) {
			genNode(nodes[i]);
		}
		genTarget(file,debugMode);
	}

	void genTarget(string file,bool d) {
		if(d) 
			LLVMWriteBitcodeToFile(this.mod,cast(const char*)("bin/"~file~".debug.be"));

		char* errors;
		LLVMTargetRef target;
    	LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &errors);

    	LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
			target, 
			LLVMGetDefaultTargetTriple(), 
			"generic", 
			LLVMGetHostCPUFeatures(),
			 LLVMCodeGenLevelDefault, 
			 LLVMRelocDefault, 
		LLVMCodeModelDefault);

		LLVMDisposeMessage(errors);

    	LLVMSetTarget(this.mod, LLVMGetDefaultTargetTriple());
    	LLVMTargetDataRef datalayout = LLVMCreateTargetDataLayout(machine);
    	char* datalayout_str = LLVMCopyStringRepOfTargetData(datalayout);
    	LLVMSetDataLayout(this.mod, datalayout_str);
    	LLVMDisposeMessage(datalayout_str);
		char* file_ptr = cast(char*)toStringz(file);
		char* file_debug_ptr = cast(char*)toStringz("bin/"~file);

    	if(!d) LLVMTargetMachineEmitToFile(machine,this.mod,file_ptr, LLVMObjectFile, &errors);
		else LLVMTargetMachineEmitToFile(machine,this.mod,file_debug_ptr, LLVMObjectFile, &errors);
	}
}