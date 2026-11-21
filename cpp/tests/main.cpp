// libotf1 test suite.
//
// Three layers, bottom up: the JSON parser every packet passes through, the
// timestamp conversions, and the OTF-1 codec itself. The codec tests run
// over this repository's own examples/ and tests/invalid/ -- the same
// corpus the JSON Schema is validated against.

#include "check.hpp"

int main() {
    std::printf("libotf1 tests\n\n");

    runJsonTests();
    runTimeTests();
    runOtf1Tests();

    std::printf("\n%d checks, %d failures\n", check::checks(), check::failures());
    return check::failures() == 0 ? 0 : 1;
}
