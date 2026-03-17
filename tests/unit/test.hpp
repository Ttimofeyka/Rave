/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <iostream>
#include <string>
#include <vector>

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

class TestRunner {
public:
    std::vector<TestResult> results;

    void run(const std::string& name, bool condition, const std::string& message = "") {
        results.push_back({name, condition, message});
        if (condition) {
            std::cout << "\033[0;32m✓\033[0m " << name << "\n";
        } else {
            std::cout << "\033[0;31m✗\033[0m " << name;
            if (!message.empty()) std::cout << ": " << message;
            std::cout << "\n";
        }
    }

    int summary() {
        int passed = 0;
        for (const auto& r : results) if (r.passed) passed++;

        std::cout << "\n" << passed << "/" << results.size() << " tests passed\n";
        return (passed == results.size()) ? 0 : 1;
    }
};

#define TEST(name) test.run(name,
#define EXPECT_TRUE(cond) cond, #cond)
#define EXPECT_EQ(a, b) (a) == (b), std::string(#a) + " == " + #b)
#define EXPECT_NE(a, b) (a) != (b), std::string(#a) + " != " + #b)
#define EXPECT_NOT_NULL(ptr) (ptr) != nullptr, #ptr " != nullptr")
