/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "test.hpp"
#include "../../src/include/parser/ast.hpp"
#include "../../src/include/parser/nodes/NodeInt.hpp"
#include "../../src/include/parser/nodes/NodeBool.hpp"
#include "../../src/include/parser/nodes/NodeFloat.hpp"
#include "../../src/include/parser/nodes/NodeString.hpp"
#include "../../src/include/parser/nodes/NodeChar.hpp"
#include "../../src/include/parser/nodes/NodeNull.hpp"
#include "../../src/include/parser/nodes/NodeBinary.hpp"
#include "../../src/include/parser/nodes/NodeUnary.hpp"
#include "../../src/include/parser/nodes/NodeArray.hpp"
#include "../../src/include/parser/nodes/NodeSizeof.hpp"
#include "../../src/include/parser/nodes/NodeType.hpp"
#include "../../src/include/parser/nodes/NodePtoi.hpp"
#include "../../src/include/parser/nodes/NodeItop.hpp"
#include "../../src/include/parser/nodes/NodeComptime.hpp"
#include "../../src/include/parser/nodes/NodeBreak.hpp"
#include "../../src/include/parser/nodes/NodeContinue.hpp"
#include "../../src/include/lexer/tokens.hpp"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

std::string exePath = "";

void initBasicTypes() {
    basicTypes[BasicType::Bool] = new TypeBasic(BasicType::Bool);
    basicTypes[BasicType::Char] = new TypeBasic(BasicType::Char);
    basicTypes[BasicType::Uchar] = new TypeBasic(BasicType::Uchar);
    basicTypes[BasicType::Short] = new TypeBasic(BasicType::Short);
    basicTypes[BasicType::Ushort] = new TypeBasic(BasicType::Ushort);
    basicTypes[BasicType::Int] = new TypeBasic(BasicType::Int);
    basicTypes[BasicType::Uint] = new TypeBasic(BasicType::Uint);
    basicTypes[BasicType::Long] = new TypeBasic(BasicType::Long);
    basicTypes[BasicType::Ulong] = new TypeBasic(BasicType::Ulong);
    basicTypes[BasicType::Cent] = new TypeBasic(BasicType::Cent);
    basicTypes[BasicType::Ucent] = new TypeBasic(BasicType::Ucent);
    basicTypes[BasicType::Float] = new TypeBasic(BasicType::Float);
    basicTypes[BasicType::Double] = new TypeBasic(BasicType::Double);
    basicTypes[BasicType::Real] = new TypeBasic(BasicType::Real);
    typeVoid = new TypeVoid();
}

void setupLLVM() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
}

int main() {
    TestRunner test;

    setupLLVM();
    initBasicTypes();

    genSettings settings;
    nlohmann::json options;

    generator = new LLVMGen("test", settings, options);

    // Test NodeInt generation
    {
        NodeInt* node = new NodeInt(BigInt("42"), 10);
        RaveValue result = node->generate();
        TEST("NodeInt generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeInt has correct type") EXPECT_NOT_NULL(result.type);
        TEST("NodeInt type is Int") EXPECT_EQ(LLVMGetIntTypeWidth(LLVMTypeOf(result.value)), 32);
        delete node;
    }

    // Test NodeBool generation
    {
        NodeBool* nodeTrue = new NodeBool(true);
        RaveValue resultTrue = nodeTrue->generate();
        TEST("NodeBool(true) generates value") EXPECT_NOT_NULL(resultTrue.value);
        TEST("NodeBool(true) has correct type") EXPECT_NOT_NULL(resultTrue.type);
        TEST("NodeBool type width is 1") EXPECT_EQ(LLVMGetIntTypeWidth(LLVMTypeOf(resultTrue.value)), 1);

        NodeBool* nodeFalse = new NodeBool(false);
        RaveValue resultFalse = nodeFalse->generate();
        TEST("NodeBool(false) generates value") EXPECT_NOT_NULL(resultFalse.value);

        delete nodeTrue;
        delete nodeFalse;
    }

    // Test NodeFloat generation
    {
        NodeFloat* node = new NodeFloat("3.14");
        RaveValue result = node->generate();
        TEST("NodeFloat generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeFloat has correct type") EXPECT_NOT_NULL(result.type);
        delete node;
    }

    // Test NodeString generation
    {
        NodeString* node = new NodeString("hello", false);
        RaveValue result = node->generate();
        TEST("NodeString generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeString has correct type") EXPECT_NOT_NULL(result.type);
        delete node;
    }

    // Test NodeChar generation
    {
        NodeChar* node = new NodeChar("A", false);
        RaveValue result = node->generate();
        TEST("NodeChar generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeChar has correct type") EXPECT_NOT_NULL(result.type);
        TEST("NodeChar type is i8") EXPECT_EQ(LLVMGetIntTypeWidth(LLVMTypeOf(result.value)), 8);
        delete node;
    }

    // Test NodeNull generation
    {
        NodeNull* node = new NodeNull(basicTypes[BasicType::Int], 0);
        RaveValue result = node->generate();
        TEST("NodeNull generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeNull has correct type") EXPECT_NOT_NULL(result.type);
        TEST("NodeNull is null constant") EXPECT_TRUE(LLVMIsNull(result.value));
        delete node;
    }

    // Test NodeBinary compile-time evaluation
    {
        NodeInt* left = new NodeInt(BigInt("10"), 10);
        NodeInt* right = new NodeInt(BigInt("5"), 10);
        NodeBinary* add = new NodeBinary(TokType::Plus, left, right, 0, false);
        Node* result = add->comptime();
        TEST("NodeBinary comptime addition") EXPECT_TRUE(instanceof<NodeInt>(result));
        if (instanceof<NodeInt>(result)) {
            TEST("NodeBinary 10+5=15") EXPECT_EQ(((NodeInt*)result)->value.to_string(), "15");
        }
        delete add;
    }

    {
        NodeInt* left = new NodeInt(BigInt("20"), 10);
        NodeInt* right = new NodeInt(BigInt("10"), 10);
        NodeBinary* cmp = new NodeBinary(TokType::More, left, right, 0, false);
        Node* result = cmp->comptime();
        TEST("NodeBinary comptime comparison") EXPECT_TRUE(instanceof<NodeBool>(result));
        if (instanceof<NodeBool>(result)) {
            TEST("NodeBinary 20>10=true") EXPECT_TRUE(((NodeBool*)result)->value);
        }
        delete cmp;
    }

    // Test NodeArray generation
    {
        std::vector<Node*> elements;
        elements.push_back(new NodeInt(BigInt("1"), 10));
        elements.push_back(new NodeInt(BigInt("2"), 10));
        elements.push_back(new NodeInt(BigInt("3"), 10));
        NodeArray* node = new NodeArray(0, elements);
        RaveValue result = node->generate();
        TEST("NodeArray generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeArray has correct type") EXPECT_NOT_NULL(result.type);
        delete node;
    }

    // Test NodeUnary comptime evaluation
    {
        NodeInt* base = new NodeInt(BigInt("42"), 10);
        NodeUnary* neg = new NodeUnary(0, TokType::Minus, base);
        Node* result = neg->comptime();
        TEST("NodeUnary comptime negation") EXPECT_TRUE(instanceof<NodeInt>(result));
        if (instanceof<NodeInt>(result)) {
            TEST("NodeUnary -42") EXPECT_EQ(((NodeInt*)result)->value.to_string(), "-42");
        }
        delete neg;
    }

    {
        NodeBool* base = new NodeBool(true);
        NodeUnary* notOp = new NodeUnary(0, TokType::Ne, base);
        Node* result = notOp->comptime();
        TEST("NodeUnary comptime not") EXPECT_TRUE(instanceof<NodeBool>(result));
        if (instanceof<NodeBool>(result)) {
            TEST("NodeUnary !true=false") EXPECT_EQ(((NodeBool*)result)->value, false);
        }
        delete notOp;
    }

    // Test NodeSizeof
    {
        NodeType* intType = new NodeType(basicTypes[BasicType::Int], 0);
        NodeSizeof* node = new NodeSizeof(intType, 0);
        RaveValue result = node->generate();
        TEST("NodeSizeof generates value") EXPECT_NOT_NULL(result.value);
        TEST("NodeSizeof has correct type") EXPECT_NOT_NULL(result.type);
        delete node;
    }

    // Test more NodeBinary comparisons
    {
        NodeInt* left = new NodeInt(BigInt("5"), 10);
        NodeInt* right = new NodeInt(BigInt("10"), 10);
        NodeBinary* less = new NodeBinary(TokType::Less, left, right, 0, false);
        Node* result = less->comptime();
        TEST("NodeBinary comptime less") EXPECT_TRUE(instanceof<NodeBool>(result));
        if (instanceof<NodeBool>(result)) {
            TEST("NodeBinary 5<10=true") EXPECT_TRUE(((NodeBool*)result)->value);
        }
        delete less;
    }

    {
        NodeInt* left = new NodeInt(BigInt("10"), 10);
        NodeInt* right = new NodeInt(BigInt("10"), 10);
        NodeBinary* eq = new NodeBinary(TokType::Equal, left, right, 0, false);
        Node* result = eq->comptime();
        TEST("NodeBinary comptime equal") EXPECT_TRUE(instanceof<NodeBool>(result));
        if (instanceof<NodeBool>(result)) {
            TEST("NodeBinary 10==10=true") EXPECT_TRUE(((NodeBool*)result)->value);
        }
        delete eq;
    }

    // Test NodeBinary logical operations
    {
        NodeBool* left = new NodeBool(true);
        NodeBool* right = new NodeBool(false);
        NodeBinary* andOp = new NodeBinary(TokType::And, left, right, 0, false);
        Node* result = andOp->comptime();
        TEST("NodeBinary comptime and") EXPECT_TRUE(instanceof<NodeBool>(result));
        if (instanceof<NodeBool>(result)) {
            TEST("NodeBinary true&&false=false") EXPECT_EQ(((NodeBool*)result)->value, false);
        }
        delete andOp;
    }

    // Test NodeComptime
    {
        NodeInt* intNode = new NodeInt(BigInt("100"), 10);
        NodeComptime* node = new NodeComptime(intNode);
        Node* result = node->comptime();
        TEST("NodeComptime comptime") EXPECT_NOT_NULL(result);
        TEST("NodeComptime returns wrapped node") EXPECT_TRUE(instanceof<NodeInt>(result));
        delete node;
    }

    // Test NodeBreak
    {
        NodeBreak* node = new NodeBreak(0);
        Type* type = node->getType();
        TEST("NodeBreak getType") EXPECT_NOT_NULL(type);
        TEST("NodeBreak type is void") EXPECT_TRUE(instanceof<TypeVoid>(type));
        Node* ct = node->comptime();
        TEST("NodeBreak comptime returns self") EXPECT_EQ(ct, node);
        delete node;
    }

    // Test NodeContinue
    {
        NodeContinue* node = new NodeContinue(0);
        Type* type = node->getType();
        TEST("NodeContinue getType") EXPECT_NOT_NULL(type);
        TEST("NodeContinue type is void") EXPECT_TRUE(instanceof<TypeVoid>(type));
        Node* ct = node->comptime();
        TEST("NodeContinue comptime returns self") EXPECT_EQ(ct, node);
        delete node;
    }

    // Test Node copy methods
    {
        NodeInt* original = new NodeInt(BigInt("42"), 10);
        Node* copied = original->copy();
        TEST("NodeInt copy") EXPECT_NOT_NULL(copied);
        TEST("NodeInt copy is NodeInt") EXPECT_TRUE(instanceof<NodeInt>(copied));
        if (instanceof<NodeInt>(copied)) {
            TEST("NodeInt copy has same value") EXPECT_EQ(((NodeInt*)copied)->value.to_string(), "42");
        }
        delete original;
        delete copied;
    }

    delete generator;

    return test.summary();
}
