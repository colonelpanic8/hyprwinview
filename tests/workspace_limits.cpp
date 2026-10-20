#include "workspace/limits.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

static void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static void testPositiveCount() {
    expect(HT::positive_count(3) == 3, "valid count changed");
    expect(HT::positive_count(1) == 1, "minimum count changed");
    expect(HT::positive_count(0) == 1, "zero count accepted");
    expect(HT::positive_count(-5) == 1, "negative count accepted");
    expect(HT::positive_count(int64_t{1} << 32) == HT::MAX_GRID_DIMENSION,
           "oversized grid dimension accepted");
}

static void testPositiveDistance() {
    expect(HT::positive_distance(300.0) == 300.0, "valid distance changed");
    expect(HT::positive_distance(0.0) == 1.0, "zero distance accepted");
    expect(HT::positive_distance(-10.0) == 1.0, "negative distance accepted");
    expect(HT::positive_distance(0.5) == 1.0, "sub-unit distance accepted");
    expect(HT::positive_distance(std::numeric_limits<double>::quiet_NaN()) == 1.0,
           "NaN distance accepted");
    expect(HT::positive_distance(std::numeric_limits<double>::infinity()) == 1.0,
           "infinite distance accepted");
}

// Mirrors HTView: hide installs an end callback, a reopen retargets the same variable before
// that callback runs, and the stale callback must not mark the reopened view inactive.
static void testStaleCloseCallback() {
    HT::CTransition transition;
    bool            active = true;

    const auto      HIDE    = transition.begin();
    auto            on_hide = [&] {
        if (transition.is_current(HIDE))
            active = false;
    };

    transition.begin();
    active = true;
    on_hide();
    expect(active, "stale close callback closed a reopened view");

    const auto HIDE_AGAIN = transition.begin();
    if (transition.is_current(HIDE_AGAIN))
        active = false;
    expect(!active, "current close callback was ignored");
}

int main() {
    testPositiveCount();
    testPositiveDistance();
    testStaleCloseCallback();
    std::cout << "workspace limits tests passed\n";
}
