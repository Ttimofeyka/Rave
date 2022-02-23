module parser.gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import parser.typesystem, parser.util;
import parser.ast;
import std.conv : to;
import std.string;

class GenerationContext {
    LLVMExecutionEngineRef engine;
    LLVMModuleRef mod;
	AtstTypeContext typecontext;
	LLVMValueRef[string] global_vars;
	LLVMBuilderRef currbuilder;

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

	void genNode(AstNode node) {
		if(node.instanceof!(AstNodeFunction) || node.instanceof!(AstNodeDecl)) {
			node.gen(this);
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

		char* err;
		LLVMPrintModuleToFile(this.mod, "tmp.ll", &err);

    	if(!d) LLVMTargetMachineEmitToFile(machine,this.mod,file_ptr, LLVMObjectFile, &errors);
		else LLVMTargetMachineEmitToFile(machine,this.mod,file_debug_ptr, LLVMObjectFile, &errors);
	}
}