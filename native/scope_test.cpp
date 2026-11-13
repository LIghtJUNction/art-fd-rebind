// SPDX-License-Identifier: Apache-2.0
#include "scope.hpp"
#include <cassert>

int main() {
    const std::string prefix = "/data/adb/art_fd_rebind/isolated_targets/";
    assert(isolatedScopeMarker(99073, "com.example:iso:Service:instance", false) == prefix + "0/com.example");
    assert(isolatedScopeMarker(90000, "com.example:appZ:Service", false) == prefix + "0/com.example");
    assert(isolatedScopeMarker(199073, "com.example:iso", false) == prefix + "1/com.example");
    assert(isolatedScopeMarker(99000, "com.example_zygote", true).empty());
    assert(isolatedScopeMarker(99000, "com.example", false).empty());
    assert(isolatedScopeMarker(99000, "com.example:com.other.Service:instance", false).empty());
    assert(isolatedScopeMarker(99000, "com.example:iso.with.dots:instance", false).empty());
    assert(isolatedScopeMarker(10435, "com.example:iso", false).empty());
    assert(isolatedScopeMarker(99073, "com.other:iso", false) != prefix + "0/com.example");
    for (const auto* bad : {"", "..", "../escape", "com..example", ".com.example", "com.example.", "com/escape", "1com.example"})
        assert(isolatedScopeMarker(99073, bad, false).empty());
}
