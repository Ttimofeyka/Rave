module llvm.types;

public import std.stdint : uintptr_t;

import llvm.config;
import core.stdc.stdint;

/+ Analysis +/

alias LLVMVerifierFailureAction = int;

/+ Transforms +/

/++ Interprocedural transformations ++/

static if (LLVM_Version >= asVersion(10, 0, 0))
{
	alias MustPreserveCallback = extern(C) LLVMBool function(LLVMValueRef, void*);
}

/++ Pass manager builder ++/

struct LLVMOpaquePassManagerBuilder; alias LLVMPassManagerBuilderRef = LLVMOpaquePassManagerBuilder*;

/+ Core +/

static if (LLVM_Version >= asVersion(3, 4, 0))
{
	alias LLVMFatalErrorHandler = extern(C) void function(const char* Reason);
}

static if (LLVM_Version >= asVersion(3, 5, 0))
{
	//This is here because putting it where it semantically belongs creates a forward reference issues.
	struct LLVMOpaqueDiagnosticInfo; alias LLVMDiagnosticInfoRef = LLVMOpaqueDiagnosticInfo*;

	alias LLVMDiagnosticHandler = extern(C) void function(LLVMDiagnosticInfoRef, void*);
	alias LLVMYieldCallback = extern(C) void function(LLVMContextRef, void *);
}

/++ Types and Enumerations ++/

alias LLVMBool = int;
struct LLVMOpaqueContext; alias LLVMContextRef = LLVMOpaqueContext*;
struct LLVMOpaqueModule; alias LLVMModuleRef = LLVMOpaqueModule*;
struct LLVMOpaqueType; alias LLVMTypeRef = LLVMOpaqueType*;
struct LLVMOpaqueValue; alias LLVMValueRef = LLVMOpaqueValue*;
static if (LLVM_Version >= asVersion(5, 0, 0)) {
	struct LLVMOpaqueMetadata; alias LLVMMetadataRef = LLVMOpaqueMetadata*;
}
static if (LLVM_Version >= asVersion(8, 0, 0)) {
	struct LLVMOpaqueNamedMDNode; alias LLVMNamedMDNodeRef = LLVMOpaqueNamedMDNode*;

	struct LLVMOpaqueValueMetadataEntry; alias LLVMValueMetadataEntry = LLVMOpaqueValueMetadataEntry*;
}
struct LLVMOpaqueBasicBlock; alias LLVMBasicBlockRef = LLVMOpaqueBasicBlock*;
static if (LLVM_Version >= asVersion(5, 0, 0)) {
	struct LLVMOpaqueDIBuilder; alias LLVMDIBuilderRef = LLVMOpaqueDIBuilder*;
}
struct LLVMOpaqueBuilder; alias LLVMBuilderRef = LLVMOpaqueBuilder*;
struct LLVMOpaqueModuleProvider; alias LLVMModuleProviderRef = LLVMOpaqueModuleProvider*;
struct LLVMOpaqueMemoryBuffer; alias LLVMMemoryBufferRef = LLVMOpaqueMemoryBuffer*;
struct LLVMOpaquePassManager; alias LLVMPassManagerRef = LLVMOpaquePassManager*;
struct LLVMOpaquePassRegistry; alias LLVMPassRegistryRef = LLVMOpaquePassRegistry*;
struct LLVMOpaqueUse; alias LLVMUseRef = LLVMOpaqueUse*;

static if (LLVM_Version >= asVersion(3, 9, 0)) {
	struct LLVMOpaqueAttributeRef; alias LLVMAttributeRef = LLVMOpaqueAttributeRef*;
}

alias LLVMAttribute = long;
alias LLVMOpcode = int;
alias LLVMTypeKind = int;
alias LLVMLinkage = int;
alias LLVMVisibility = int;
alias LLVMDLLStorageClass = int;
alias LLVMCallConv = int;
alias LLVMIntPredicate = int;
alias LLVMRealPredicate = int;
alias LLVMLandingPadClauseTy = int;
static if (LLVM_Version >= asVersion(3, 3, 0))
{
	alias LLVMThreadLocalMode = int;
	alias LLVMAtomicOrdering = int;
	alias LLVMAtomicRMWBinOp = int;
}
static if (LLVM_Version >= asVersion(3, 5, 0))
{
	alias LLVMDiagnosticSeverity = int;
}
static if (LLVM_Version >= asVersion(3, 9, 0))
{
	alias LLVMValueKind = int;
	alias LLVMAttributeIndex = uint;
}
/+ Disassembler +/

alias LLVMDisasmContextRef = void*;
alias LLVMOpInfoCallback = extern(C) int function(void* DisInfo, ulong PC, ulong Offset, ulong Size, int TagType, void* TagBuf);
alias LLVMSymbolLookupCallback = extern(C) const char* function(void* DisInfo, ulong ReferenceValue, ulong* ReferenceType, ulong ReferencePC, const char** ReferenceName);

struct LLVMOpInfoSymbol1
{
	ulong Present;
	const(char)* Name;
	ulong Value;
}

struct LLVMOpInfo1
{
	LLVMOpInfoSymbol1 AddSymbol;
	LLVMOpInfoSymbol1 SubtractSymbol;
	ulong Value;
	ulong VariantKind;
}

static if (LLVM_Version < asVersion(3, 3, 0))
{
	/+ Enhanced Disassembly +/

	alias EDDisassemblerRef = void*;
	alias EDInstRef = void*;
	alias EDTokenRef = void*;
	alias EDOperandRef = void*;

	alias EDAssemblySyntax_t = int;

	alias EDByteReaderCallback = extern(C) int function(ubyte* Byte, ulong address, void* arg);
	alias EDRegisterReaderCallback = extern(C) int function(ulong* value, uint regID, void* arg);

	alias EDByteBlock_t = extern(C) int function(ubyte* Byte, ulong address);
	alias EDRegisterBlock_t = extern(C) int function(ulong* value, uint regID);
	alias EDTokenVisitor_t = extern(C) int function(EDTokenRef token);
}

/+ Execution Engine +/

struct LLVMOpaqueGenericValue; alias LLVMGenericValueRef = LLVMOpaqueGenericValue*;
struct LLVMOpaqueExecutionEngine; alias LLVMExecutionEngineRef = LLVMOpaqueExecutionEngine*;

static if (LLVM_Version >= asVersion(3, 3, 0))
{
	static if (LLVM_Version >= asVersion(3, 4, 0))
	{
		struct LLVMOpaqueMCJITMemoryManager; alias LLVMMCJITMemoryManagerRef = LLVMOpaqueMCJITMemoryManager*;

		struct LLVMMCJITCompilerOptions
		{
			uint OptLevel;
			LLVMCodeModel CodeModel;
			LLVMBool NoFramePointerElim;
			LLVMBool EnableFastISel;
			LLVMMCJITMemoryManagerRef MCJMM;
		}

		alias LLVMMemoryManagerAllocateCodeSectionCallback = extern(C) ubyte function(void* Opaque, uintptr_t Size, uint Alignment, uint SectionID, const char* SectionName);
		alias LLVMMemoryManagerAllocateDataSectionCallback = extern(C) ubyte function(void* Opaque, uintptr_t Size, uint Alignment, uint SectionID, const char* SectionName, LLVMBool IsReadOnly);
		alias LLVMMemoryManagerFinalizeMemoryCallback = extern(C) LLVMBool function(void* Opaque, char** ErrMsg);
		alias LLVMMemoryManagerDestroyCallback = extern(C) void function(void* Opaque);
	}
	else
	{
		struct LLVMMCJITCompilerOptions
		{
			uint OptLevel;
			LLVMCodeModel CodeModel;
			LLVMBool NoFramePointerElim;
			LLVMBool EnableFastISel;
		}
	}
}

static if (LLVM_Version >= asVersion(3, 2, 0))
{
	/+ Linker +/

	alias LLVMLinkerMode = int;
}

/+ Link Time Optimization +/
alias lto_bool_t = bool;
alias llvm_lto_t = void*;
alias llvm_lto_status_t = llvm_lto_status;


alias llvm_lto_status = int;

/+ LTO +/

static if (LLVM_Version >= asVersion(9, 0, 0))
{
	struct LLVMOpaqueLTOInput; alias lto_input_t = LLVMOpaqueLTOInput*;
}
static if (LLVM_Version >= asVersion(3, 5, 0))
{
	struct LLVMOpaqueLTOModule; alias lto_module_t = LLVMOpaqueLTOModule*;
}
else
{
	struct LTOModule; alias lto_module_t = LTOModule*;
}
static if (LLVM_Version >= asVersion(3, 5, 0))
{
	struct LLVMOpaqueLTOCodeGenerator; alias lto_code_gen_t = LLVMOpaqueLTOCodeGenerator*;
}
else
{
	struct LTOCodeGenerator; alias lto_code_gen_t = LTOCodeGenerator*;
}
static if (LLVM_Version >= asVersion(3, 9, 0))
{
	struct LLVMOpaqueThinLTOCodeGenerator; alias thinlto_code_gen_t = LLVMOpaqueThinLTOCodeGenerator*;
}

alias lto_symbol_attributes = int;
alias lto_debug_model = int;
alias lto_codegen_model = int;
alias lto_codegen_diagnostic_severity_t = int;
alias lto_diagnostic_handler_t = extern(C) void function(lto_codegen_diagnostic_severity_t severity, const(char)* diag, void* ctxt);

/+ Object file reading and writing +/

static if (LLVM_Version >= asVersion(9, 0, 0))
{
	alias LLVMBinaryType = int;

	struct LLVMOpaqueBinary; alias LLVMBinaryRef = LLVMOpaqueBinary*;

	deprecated("Use LLVMBinaryRef instead.") struct LLVMOpaqueObjectFile;
	deprecated("Use LLVMBinaryRef instead.") alias LLVMObjectFileRef = LLVMOpaqueObjectFile*;
}
else
{
	struct LLVMOpaqueObjectFile; alias LLVMObjectFileRef = LLVMOpaqueObjectFile*;
}
struct LLVMOpaqueSectionIterator; alias LLVMSectionIteratorRef = LLVMOpaqueSectionIterator*;
struct LLVMOpaqueSymbolIterator; alias LLVMSymbolIteratorRef = LLVMOpaqueSymbolIterator*;
struct LLVMOpaqueRelocationIterator; alias LLVMRelocationIteratorRef = LLVMOpaqueRelocationIterator*;

/+ Target information +/

struct LLVMOpaqueTargetData; alias LLVMTargetDataRef = LLVMOpaqueTargetData*;
struct LLVMOpaqueTargetLibraryInfotData; alias LLVMTargetLibraryInfoRef = LLVMOpaqueTargetLibraryInfotData*;
static if (LLVM_Version < asVersion(3, 4, 0))
{
	struct LLVMStructLayout; alias LLVMStructLayoutRef = LLVMStructLayout*;
}
alias LLVMByteOrdering = int;

/+ Target machine +/

struct LLVMOpaqueTargetMachine; alias LLVMTargetMachineRef = LLVMOpaqueTargetMachine*;
struct LLVMTarget; alias LLVMTargetRef = LLVMTarget*;

alias LLVMCodeGenOptLevel = int;
alias LLVMRelocMode = int;
alias LLVMCodeModel = int;
alias LLVMCodeGenFileType = int;

static if (LLVM_Version >= asVersion(5, 0, 0) && LLVM_Version < asVersion(7, 0, 0)) {
	struct LLVMOpaqueSharedModule; alias LLVMSharedModuleRef = LLVMOpaqueSharedModule*;
}
static if (LLVM_Version >= asVersion(5, 0, 0) && LLVM_Version < asVersion(6, 0, 0)) {
	struct LLVMOpaqueSharedObjectBuffer; alias LLVMSharedObjectBufferRef = LLVMOpaqueSharedObjectBuffer*;
}

static if (LLVM_Version >= asVersion(3, 8, 0))
{
	/+ JIT compilation of LLVM IR +/

	struct LLVMOrcOpaqueJITStack; alias LLVMOrcJITStackRef = LLVMOrcOpaqueJITStack*;
}

static if (LLVM_Version >= asVersion(7, 0, 0))
{
	alias LLVMOrcModuleHandle = uint64_t;
}
else static if (LLVM_Version >= asVersion(3, 8, 0))
{
	alias LLVMOrcModuleHandle = uint32_t;
}

static if (LLVM_Version >= asVersion(3, 8, 0))
{
	alias LLVMOrcTargetAddress = ulong;

	alias LLVMOrcSymbolResolverFn = extern(C) ulong function(const(char)* Name, void* LookupCtx);
	alias LLVMOrcLazyCompileCallbackFn = extern(C) ulong function(LLVMOrcJITStackRef JITStack, void* CallbackCtx);
}

static if (LLVM_Version >= asVersion(3, 9, 0))
{
	alias LLVMOrcErrorCode = int;

	struct LTOObjectBuffer
	{
		const(char)* Buffer;
		size_t Size;
	}
}

/+ Debug info flags +/

static if (LLVM_Version >= asVersion(6, 0, 0))
{
	alias LLVMDIFlags = int;
	alias LLVMDWARFSourceLanguage = int;
	alias LLVMDWARFEmissionKind = int;
}


static if (LLVM_Version >= asVersion(7, 0, 0))
{
	alias LLVMComdatSelectionKind = int;
	alias LLVMUnnamedAddr = int;
	alias LLVMInlineAsmDialect = int;
	alias LLVMModuleFlagBehavior = int;
	alias LLVMDWARFTypeEncoding = int;

}

static if (LLVM_Version >= asVersion(10, 0, 0))
{
	alias LLVMDWARFMacinfoRecordType = int;
}


static if (LLVM_Version >= asVersion(8, 0, 0)) {
	alias LLVMMetadataKind = uint;
}

static if (LLVM_Version >= asVersion(7, 0, 0)) {
	struct LLVMComdat; alias LLVMComdatRef = LLVMComdat*;
}

static if (LLVM_Version >= asVersion(7, 0, 0)) {
	struct LLVMOpaqueModuleFlagEntry; alias LLVMModuleFlagEntry = LLVMOpaqueModuleFlagEntry*;
}

static if (LLVM_Version >= asVersion(7, 0, 0)) {
	struct LLVMOpaqueJITEventListener; alias LLVMJITEventListenerRef = LLVMOpaqueJITEventListener*;
}

/+ Error +/

static if (LLVM_Version >= asVersion(8, 0, 0)) {
	struct LLVMOpaqueError; alias LLVMErrorRef = LLVMOpaqueError*;

	alias LLVMErrorTypeId = const void*;
}

/+ Remarks / OptRemarks +/

static if (LLVM_Version >= asVersion(9, 0, 0)) {
	struct LLVMRemarkOpaqueString; alias LLVMRemarkStringRef = LLVMRemarkOpaqueString*;
	struct LLVMRemarkOpaqueEntry; alias LLVMRemarkEntryRef = LLVMRemarkOpaqueEntry*;
	struct LLVMRemarkOpaqueParser; alias LLVMRemarkParserRef = LLVMRemarkOpaqueParser*;
	struct LLVMRemarkOpaqueArg; alias LLVMRemarkArgRef = LLVMRemarkOpaqueArg*;
	struct LLVMRemarkOpaqueDebugLoc; alias LLVMRemarkDebugLocRef = LLVMRemarkOpaqueDebugLoc*;

	alias LLVMRemarkType = int;
} else static if (LLVM_Version >= asVersion(8, 0, 0)) {
	struct LLVMOptRemarkStringRef
	{
		const(char)* Str;
		uint32_t Len;
	}

	struct LLVMOptRemarkDebugLoc
	{
		// File:
		LLVMOptRemarkStringRef SourceFile;
		// Line:
		uint32_t SourceLineNumber;
		// Column:
		uint32_t SourceColumnNumber;
	}

	struct LLVMOptRemarkArg
	{
		// e.g. "Callee"
		LLVMOptRemarkStringRef Key;
		// e.g. "malloc"
		LLVMOptRemarkStringRef Value;

		// "DebugLoc": Optional
		LLVMOptRemarkDebugLoc DebugLoc;
	}

	struct LLVMOptRemarkEntry
	{
		// e.g. !Missed, !Passed
		LLVMOptRemarkStringRef RemarkType;
		// "Pass": Required
		LLVMOptRemarkStringRef PassName;
		// "Name": Required
		LLVMOptRemarkStringRef RemarkName;
		// "Function": Required
		LLVMOptRemarkStringRef FunctionName;

		// "DebugLoc": Optional
		LLVMOptRemarkDebugLoc DebugLoc;
		// "Hotness": Optional
		uint32_t Hotness;
		// "Args": Optional. It is an array of `num_args` elements.
		uint32_t NumArgs;
		LLVMOptRemarkArg* Args;
	}

	struct LLVMOptRemarkOpaqueParser; alias LLVMOptRemarkParserRef = LLVMOptRemarkOpaqueParser*;
}
