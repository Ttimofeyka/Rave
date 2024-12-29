/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>

class Type {
public:
    virtual int getSize() = 0;
    virtual Type* check(Type* parent) = 0;
    virtual std::string toString() = 0;
    virtual Type* copy() = 0;
    virtual Type* getElType() = 0;
    virtual ~Type();
};