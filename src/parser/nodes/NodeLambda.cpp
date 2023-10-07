/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/utils.hpp"
#include "../../include/parser/nodes/NodeLambda.hpp"

NodeLambda::NodeLambda(long loc, TypeFunc* tf, NodeBlock* block, std::string name) {
    this->loc = loc;
    this->tf = tf;
    this->block = block;
    this->name = name;
}