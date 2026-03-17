/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <vector>
#include "Types.hpp"

class NodeCall;
class NodeInt;
struct RaveValue;
struct FuncArgSet;

// Type conversion utilities
std::string typeToString(Type* arg);
std::string typesToString(std::vector<Type*>& args);
std::string typesToString(std::vector<FuncArgSet>& args);

// Type helper functions
TypeFunc* callToTFunc(NodeCall* call);
std::vector<Type*> parametersToTypes(std::vector<RaveValue> params);
