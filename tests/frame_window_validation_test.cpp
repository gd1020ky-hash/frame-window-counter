#include "../src/Data/FrameWindowValidation.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct TestAction {
    int frame;
    int id;
};

static int failures = 0;

static void check(bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        ++failures;
    }
    else {
        std::cout << "PASS: " << name << '\n';
    }
}

static bool near(
    double a,
    double b,
    double epsilon = 1e-9
) {
    return std::abs(a - b) <= epsilon;
}

int main() {
    using namespace afwc;

    check(
        near(normalizeFrameWindow(0.5), 0.5),
        "valid frame window preserved"
    );

    check(
        near(normalizeFrameWindow(0.0), 1.0),
        "zero window falls back to 1"
    );

    check(
        near(normalizeFrameWindow(-2.0), 1.0),
        "negative window falls back to 1"
    );

    check(
        near(
            normalizeFrameWindow(
                std::numeric_limits<double>::quiet_NaN()
            ),
            1.0
        ),
        "NaN window falls back to 1"
    );

    check(
        near(
            normalizeFrameWindow(
                std::numeric_limits<double>::infinity()
            ),
            1.0
        ),
        "infinite window falls back to 1"
    );

    check(
        normalizeIfCount(1) == 1,
        "one input per frame preserved"
    );

    check(
        normalizeIfCount(4) == 4,
        "four inputs per frame preserved"
    );

    check(
        normalizeIfCount(0) == 1,
        "zero I/F becomes one"
    );

    check(
        normalizeIfCount(-5) == 1,
        "negative I/F becomes one"
    );

    check(
        normalizeIfCount(MAX_IF_COUNT + 1) == MAX_IF_COUNT,
        "I/F safety limit applied"
    );

    const auto four = expandFrameWindow(10.0, 1.0, 4);

    check(
        four.size() == 4,
        "four I/F expands to four inputs"
    );

    check(
        near(four[0].frame, 10.0),
        "main input stays on base frame"
    );

    check(
        near(four[1].frame, 10.25),
        "first sub-input frame"
    );

    check(
        near(four[2].frame, 10.50),
        "second sub-input frame"
    );

    check(
        near(four[3].frame, 10.75),
        "third sub-input frame"
    );

    for (const auto& input : four) {
        check(
            std::isfinite(input.frame) &&
            std::isfinite(input.window) &&
            input.window > 0.0,
            "expanded input has valid values"
        );
    }

    std::vector<TestAction> actions = {
        {12, 1},
        {12, 2},
        {11, 3},
        {12, 4}
    };

    stableSortByFrame(actions);

    check(
        actions.size() == 4,
        "same-frame actions are not merged"
    );

    check(
        actions[0].frame == 11 && actions[0].id == 3,
        "earlier frame sorts first"
    );

    check(
        actions[1].id == 1 &&
        actions[2].id == 2 &&
        actions[3].id == 4,
        "same-frame action order is stable"
    );

    const auto first = expandFrameWindow(20.0, 1.0, 2);
    const auto second = expandFrameWindow(20.0, 1.0, 3);

    check(
        first.size() + second.size() == 5,
        "same-frame I/F actions expand independently"
    );

    if (failures != 0) {
        std::cerr << "\n"
                  << failures
                  << " test(s) failed.\n";

        return 1;
    }

    std::cout << "\nAll offline validation tests passed.\n";

    return 0;
}