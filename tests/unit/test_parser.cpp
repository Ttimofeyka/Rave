/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "test.hpp"
#include "../../src/include/parser/parser.hpp"
#include "../../src/include/parser/ParserUtils.hpp"
#include "../../src/include/parser/TypeUtils.hpp"
#include "../../src/include/parser/Types.hpp"
#include "../../src/include/utils.hpp"
#include "../../src/include/lexer/lexer.hpp"
#include "../../src/include/parser/nodes/NodeInt.hpp"
#include "../../src/include/parser/nodes/NodeBinary.hpp"
#include "../../src/include/parser/nodes/NodeIden.hpp"

// Required global variables
std::string exePath = "./";

void initBasicTypes() {
    basicTypes[BasicType::Bool] = new TypeBasic(BasicType::Bool);
    basicTypes[BasicType::Char] = new TypeBasic(BasicType::Char);
    basicTypes[BasicType::Int] = new TypeBasic(BasicType::Int);
    basicTypes[BasicType::Long] = new TypeBasic(BasicType::Long);
    basicTypes[BasicType::Float] = new TypeBasic(BasicType::Float);
    basicTypes[BasicType::Double] = new TypeBasic(BasicType::Double);
    typeVoid = new TypeVoid();
    pointerSize = 64;
}

int main() {
    TestRunner test;
    initBasicTypes();

    // Test ParserUtils
    TEST("isBasicType recognizes int") EXPECT_TRUE(isBasicType("int"));
    TEST("isBasicType recognizes float") EXPECT_TRUE(isBasicType("float"));
    TEST("isBasicType rejects unknown") EXPECT_TRUE(!isBasicType("unknown"));

    // Test Type parsing
    TEST("TypeBasic int size") EXPECT_EQ(basicTypes[BasicType::Int]->getSize(), 32);
    TEST("TypeBasic long size") EXPECT_EQ(basicTypes[BasicType::Long]->getSize(), 64);
    TEST("TypeBasic float is float") EXPECT_TRUE(basicTypes[BasicType::Float]->isFloat());
    TEST("TypeBasic int not float") EXPECT_TRUE(!basicTypes[BasicType::Int]->isFloat());

    // Test TypePointer
    TypePointer* intPtr = new TypePointer(basicTypes[BasicType::Int]);
    TEST("TypePointer toString") EXPECT_EQ(intPtr->toString(), std::string("int*"));
    TEST("TypePointer getSize") EXPECT_EQ(intPtr->getSize(), 64);
    TEST("TypePointer getElType") EXPECT_EQ(intPtr->getElType(), basicTypes[BasicType::Int]);

    // Test TypeArray
    NodeInt* arraySize = new NodeInt(BigInt(10));
    TypeArray* intArray = new TypeArray(arraySize, basicTypes[BasicType::Int]);
    TEST("TypeArray getSize") EXPECT_EQ(intArray->getSize(), 320);
    TEST("TypeArray getElType") EXPECT_EQ(intArray->getElType(), basicTypes[BasicType::Int]);

    // Test Parser token handling
    std::vector<Token*> tokens = {
        new Token(TokType::Number, "42", 1),
        new Token(TokType::Eof, "", 1)
    };
    Parser parser(tokens, "test.rave");
    TEST("Parser peek") EXPECT_EQ(parser.peek()->type, TokType::Number);
    TEST("Parser peek value") EXPECT_EQ(parser.peek()->value, std::string("42"));

    parser.next();
    TEST("Parser next") EXPECT_EQ(parser.peek()->type, TokType::Eof);

    // Test simple expression parsing
    std::vector<Token*> exprTokens = {
        new Token(TokType::Number, "5", 1),
        new Token(TokType::Plus, "+", 1),
        new Token(TokType::Number, "3", 1),
        new Token(TokType::Eof, "", 1)
    };
    Parser exprParser(exprTokens, "test.rave");
    Node* expr = exprParser.parseExpr("");
    TEST("Parse binary expression") EXPECT_NOT_NULL(expr);
    TEST("Binary expression is NodeBinary") EXPECT_TRUE(instanceof<NodeBinary>(expr));

    // Test identifier parsing
    std::vector<Token*> idenTokens = {
        new Token(TokType::Identifier, "myVar", 1),
        new Token(TokType::Eof, "", 1)
    };
    Parser idenParser(idenTokens, "test.rave");
    Node* iden = idenParser.parseAtom("");
    TEST("Parse identifier") EXPECT_NOT_NULL(iden);
    TEST("Identifier is NodeIden") EXPECT_TRUE(instanceof<NodeIden>(iden));
    if (instanceof<NodeIden>(iden)) {
        TEST("Identifier name") EXPECT_EQ(((NodeIden*)iden)->name, std::string("myVar"));
    }

    // Test TypeUtils
    std::string intStr = typeToString(basicTypes[BasicType::Int]);
    TEST("typeToString int") EXPECT_EQ(intStr, std::string("i"));

    std::string floatStr = typeToString(basicTypes[BasicType::Float]);
    TEST("typeToString float") EXPECT_EQ(floatStr, std::string("f"));

    TypePointer* charPtr = new TypePointer(basicTypes[BasicType::Char]);
    std::string ptrStr = typeToString(charPtr);
    TEST("typeToString pointer") EXPECT_EQ(ptrStr, std::string("pc"));

    return test.summary();
}
