module parser.generator.llvm_abs;

import parser.generator.gen;
import llvm;
import std.string;
import std.array;
import std.uni;
import std.ascii;
import std.conv : to;
import std.stdio : writeln;
import parser.atst;
import parser.analyzer;
import lexer.tokens;

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

LLVMValueRef createGlobal(GenerationContext ctx, LLVMTypeRef type, LLVMValueRef val, string name, bool isExt) {
		LLVMValueRef global = LLVMAddGlobal(
			ctx.mod,
			type,
			toStringz("_Raveg"~to!string(name.length)~name)
		);
		if(isExt) LLVMSetLinkage(global, LLVMAvailableExternallyLinkage);
		if(val !is null && !isExt) LLVMSetInitializer(global,val);
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
        LLVMValueRef oneg;
		LLVMValueRef twog;

		if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
			if(!isReal(one)) {
				oneg = one;
				twog = LLVMBuildIntCast(
					ctx.currbuilder,
					two,
					LLVMTypeOf(oneg),
					toStringz("intcast")
				);
			}
			else {
				oneg = one; twog = two;
			}
		}
		else {oneg = one; twog = two;}
		
		if(!isReal(one) && !isReal(two)) return LLVMBuildAdd(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("operaddi_result")
		);

		return LLVMBuildFAdd(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("operaddf_result")
		);
}

LLVMValueRef operSub(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
        LLVMValueRef oneg;
		LLVMValueRef twog;

		if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
			if(!isReal(one)) {
				oneg = one;
				twog = LLVMBuildIntCast(
					ctx.currbuilder,
					two,
					LLVMTypeOf(oneg),
					toStringz("intcast")
				);
			}
			else {
				oneg = one; twog = two;
			}
		}
		else {oneg = one; twog = two;}
		
		if(!isReal(one) && !isReal(two)) return LLVMBuildSub(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("opersubi_result")
		);

		return LLVMBuildFSub(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("opersubf_result")
		);
}

LLVMValueRef operMul(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
        LLVMValueRef oneg;
		LLVMValueRef twog;

		if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
			if(!isReal(one)) {
				oneg = one;
				twog = LLVMBuildIntCast(
					ctx.currbuilder,
					two,
					LLVMTypeOf(oneg),
					toStringz("intcast")
				);
			}
			else {
				oneg = one; twog = two;
			}
		}
		else {oneg = one; twog = two;}
		
		if(!isReal(one) && !isReal(two)) return LLVMBuildMul(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("opermuli_result")
		);

		return LLVMBuildFMul(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("opermulf_result")
		);
}

LLVMValueRef operDiv(GenerationContext ctx, LLVMValueRef one, LLVMValueRef two) {
        LLVMValueRef oneg;
		LLVMValueRef twog;

		if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
			if(!isReal(one)) {
				oneg = one;
				twog = LLVMBuildIntCast(
					ctx.currbuilder,
					two,
					LLVMTypeOf(oneg),
					toStringz("intcast")
				);
			}
			else {
				oneg = one; twog = two;
			}
		}
		else {oneg = one; twog = two;}
		
		if(!isReal(one) && !isReal(two)) return LLVMBuildSDiv(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("opersubi_result")
		);

		return LLVMBuildFDiv(
			ctx.currbuilder,
			oneg,
			twog,
			toStringz("opersubf_result")
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
		return LLVMBuildIntCast(
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

LLVMValueRef castTo(GenerationContext ctx, LLVMValueRef v, LLVMTypeRef t) {
	if(v == null) return v;
	auto kindone = LLVMGetTypeKind(LLVMTypeOf(v));
	auto kindtwo = LLVMGetTypeKind(t);
	if(kindone == LLVMIntegerTypeKind && kindtwo == LLVMPointerTypeKind) {
		return LLVMBuildIntToPtr(
			ctx.currbuilder,
			v,
			t,
			toStringz("itop_cast")
		);
	}
	else if(kindone == LLVMPointerTypeKind && kindtwo == LLVMIntegerTypeKind) {
		return LLVMBuildPtrToInt(
			ctx.currbuilder,
			v,
			t,
			toStringz("ptoi_cast")
		);
	}
	else if(kindone == LLVMIntegerTypeKind && kindtwo == LLVMIntegerTypeKind) {
		return LLVMBuildIntCast(
			ctx.currbuilder,
			v,
			t,
			toStringz("itoi_cast")
		);
	}
	else if(kindone == LLVMIntegerTypeKind && kindtwo == LLVMFloatTypeKind) {
		return LLVMBuildSIToFP(
			ctx.currbuilder,
			v,
			t,
			toStringz("fptoi_cast")
		);
	}
	else if(kindtwo == LLVMIntegerTypeKind && kindone == LLVMFloatTypeKind) {
		return LLVMBuildFPToSI(
			ctx.currbuilder,
			v,
			t,
			toStringz("fptoi_cast")
		);
	}
	else if(kindone == LLVMArrayTypeKind && kindtwo == LLVMPointerTypeKind) {
		return LLVMBuildInBoundsGEP(
				ctx.currbuilder,
				v,
				[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
				2,
				toStringz("gep")
		);
	}
	else if(kindone == LLVMPointerTypeKind && kindtwo == LLVMPointerTypeKind) {
		// Ptr to Ptr
		return LLVMBuildPointerCast(
			ctx.currbuilder,
			v,
			t,
			toStringz("ptop")
		);
	}
	ctx.err("Illegal cast! (maybe, you did cast struct?)",SourceLocation(-1,-1,"Unknown"));
	assert(0);
}