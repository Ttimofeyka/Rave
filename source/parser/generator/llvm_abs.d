module parser.generator.llvm_abs;

import parser.generator.gen;
import llvm;
import std.string;
import std.array;
import std.uni;
import std.ascii;
import std.conv : to;

LLVMValueRef createLocal(GenerationContext ctx, LLVMTypeRef type, LLVMValueRef val, string name) {
		LLVMValueRef local = LLVMBuildAlloca(
			ctx.currbuilder,
			type,
			toStringz(name)
		);

		if(val !is null) {
			LLVMBuildStore(
				ctx.currbuilder,
				val,
				local
			);
		}

		ctx.gstack.addLocal(local,name);
		return local;
}

LLVMValueRef createGlobal(GenerationContext ctx, LLVMTypeRef type, LLVMValueRef val, string name) {
		LLVMValueRef global = LLVMAddGlobal(
			ctx.mod,
			type,
			toStringz("_Raveg"~to!string(name.length)~name)
		);
		if(val !is null) LLVMSetInitializer(global,val);
		ctx.gstack.addGlobal(global,name);
		return global;
}

void setGlobal(GenerationContext ctx, LLVMValueRef value, string name) {
		LLVMBuildStore(
			ctx.currbuilder,
			value,
			ctx.gstack[name]
		);
}

void setLocal(GenerationContext ctx, LLVMValueRef value, string name) {
		auto tmp = LLVMBuildAlloca(
			ctx.currbuilder,
			LLVMGetAllocatedType(ctx.gstack[name]),
			toStringz(name~"_setlocal")
		);

		if(value !is null) LLVMBuildStore(
			ctx.currbuilder,
			value,
			tmp
		);

		ctx.gstack.set(tmp,name);
}

LLVMValueRef getValueByPtr(GenerationContext ctx, string name) {
		return LLVMBuildLoad(
			ctx.currbuilder,
			ctx.gstack[name],
			toStringz(name~"_valbyptr")
		);
}

bool isReal(LLVMValueRef v) {
	return LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMFloatTypeKind;
}

LLVMValueRef getVarPtr(GenerationContext ctx, string name) {
		return ctx.gstack[name];
}

LLVMValueRef operAdd(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
        if(!isReal(one) && !isReal(two)) return LLVMBuildAdd(
			ctx.currbuilder,
			one,
			two,
			toStringz("operaddi_result")
		);

		return LLVMBuildFAdd(
			ctx.currbuilder,
			one,
			two,
			toStringz("operaddf_result")
		);
}

LLVMValueRef operSub(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
		if(!isReal(one) && !isReal(two)) return LLVMBuildSub(
			ctx.currbuilder,
			one,
			two,
			toStringz("opersubi_result")
		);

		return LLVMBuildFSub(
			ctx.currbuilder,
			one,
			two,
			toStringz("opersubf_result")
		);
}

LLVMValueRef operMul(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
		if(!isReal(one) && !isReal(two)) return LLVMBuildMul(
			ctx.currbuilder,
			one,
			two,
			toStringz("opermuli_result")
		);

		return LLVMBuildFMul(
			ctx.currbuilder,
			one,
			two,
			toStringz("opermulf_result")
		);
}

LLVMValueRef operDiv(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
		if(LLVMTypeOf(one) == LLVMFloatType()) return LLVMBuildFDiv(
			ctx.currbuilder,
			one,
			two,
			toStringz("operdivf_result")
		);
        return LLVMBuildSDiv(
			ctx.currbuilder,
			one,
			two,
			toStringz("operdivi_result")
		);
}

LLVMValueRef castNum(GenerationContext ctx, LLVMValueRef tocast, LLVMTypeRef type) {
		if(isReal(tocast)) {
			// real to ...?
			if(LLVMGetTypeKind(type) == LLVMIntegerTypeKind) return LLVMBuildFPToSI(
				ctx.currbuilder,
				tocast,
				type,
				toStringz("castNumFtI")
			);
			return null;
		}
		
		// int to ...?
		if(LLVMGetTypeKind(type) == LLVMFloatTypeKind) return LLVMBuildSIToFP(
			ctx.currbuilder,
			tocast,
			type,
			toStringz("castNumItF")
		);
		return LLVMBuildSExt(
			ctx.currbuilder,
			tocast,
			type,
			toStringz("castNumItI")
		);
}

LLVMValueRef castPtr(GenerationContext ctx, LLVMValueRef tocast, LLVMTypeRef type) {
	return LLVMBuildPointerCast(
		ctx.currbuilder,
		tocast,
		type,
		toStringz("castPtr")
	);
}

LLVMValueRef ptrToInt(GenerationContext ctx, LLVMValueRef ptr, LLVMTypeRef type) {
		return LLVMBuildPtrToInt(
			ctx.currbuilder,
			ptr,
			type,
			toStringz("ptrToint")
		);
}

LLVMValueRef intToPtr(GenerationContext ctx, LLVMValueRef num, LLVMTypeRef type) {
		return LLVMBuildIntToPtr(
			ctx.currbuilder,
			num,
			type,
			toStringz("intToptr")
		);
}

LLVMTypeRef getAType(LLVMValueRef v) {
		return LLVMGetAllocatedType(v);
}

LLVMTypeRef getVarType(GenerationContext ctx, string n) {
    if(ctx.gstack.isGlobal(n)) {
        return LLVMGlobalGetValueType(ctx.gstack[n]);
    }
    return getAType(ctx.gstack[n]);
}

LLVMValueRef retNull(GenerationContext ctx, LLVMTypeRef type) {
	return LLVMBuildRet(ctx.currbuilder,LLVMConstNull(type));
}

LLVMValueRef cmpNum(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
	if(!isReal(one)) return LLVMBuildICmp(
		ctx.currbuilder,
		LLVMIntEQ,
		one,
		two,
		toStringz("cmpnum_i")
	);

	else return LLVMBuildFCmp(
		ctx.currbuilder,
		LLVMRealOEQ,
		one,
		two,
		toStringz("cmpnum_f")
	);
}

LLVMValueRef ncmpNum(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
	if(!isReal(one)) return LLVMBuildICmp(
		ctx.currbuilder,
		LLVMIntNE,
		one,
		two,
		toStringz("cmpnum_i")
	);

	else return LLVMBuildFCmp(
		ctx.currbuilder,
		LLVMRealONE,
		one,
		two,
		toStringz("cmpnum_f")
	);
}

bool isTypeEqual(LLVMValueRef one, LLVMValueRef two) {
	return LLVMTypeOf(one) == LLVMTypeOf(two);
}

LLVMValueRef[] trueNums(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
	if(isTypeEqual(one,two)) return [one,two];
	LLVMValueRef two_to_one = castNum(ctx, two, LLVMTypeOf(one));
	return [one, two_to_one];
}