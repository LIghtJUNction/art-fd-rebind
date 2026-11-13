// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#pragma once
#include <sched.h>
#include <sys/mount.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "log.h"

inline std::string mountNamespaceIdentity() {
    char name[128];
    const auto n = readlink("/proc/self/ns/mnt", name, sizeof(name));
    return n > 0 ? std::string(name, static_cast<size_t>(n)) : std::string();
}

inline std::vector<std::string> oplusOverlayTargets() {
    std::vector<std::string> targets;
    std::ifstream input("/proc/self/mountinfo");
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find(" - ");
        if (separator == std::string::npos) continue;
        std::string fs;
        std::istringstream(line.substr(separator + 3)) >> fs;
        if (fs != "overlay") continue;
        std::string id, parent, dev, root, point;
        if (!(std::istringstream(line) >> id >> parent >> dev >> root >> point)) continue;
        if (point.starts_with("/system_ext/framework/oplusex/") ||
            point.starts_with("/system_ext/lib64/oplusex/")) targets.push_back(point);
    }
    std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) {
        return a.size() > b.size();
    });
    return targets;
}

// NoHello's namespace-isolation + MNT_DETACH approach, limited to an opted-in
// application's Oplus overlays. Never unmount without a NEW PRIVATE namespace.
inline int isolateOplusOverlays() {
    const auto targets = oplusOverlayTargets();
    if (targets.empty()) return 0;
    const auto before = mountNamespaceIdentity();
    if (before.empty() || unshare(CLONE_NEWNS) != 0) return -1;
    const auto after = mountNamespaceIdentity();
    if (after.empty() || after == before) return -1;
    if (mount(nullptr, "/", nullptr, MS_PRIVATE | MS_REC, nullptr) != 0) return -1;
    int removed = 0;
    for (const auto& target : targets) {
        if (umount2(target.c_str(), MNT_DETACH) == 0) ++removed;
        else LOGE("Oplus detach failed path=%s errno=%d", target.c_str(), errno);
    }
    return removed;
}
