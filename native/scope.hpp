// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#pragma once
#include <cstdint>
#include <string>

struct ScopeQuery {
    int32_t uid = -1;
    int32_t pid = -1;
    int32_t childZygote = 0;
    char process[256]{};
};

inline bool isIsolatedUid(int uid) {
    return uid >= 0 && uid % 100000 >= 90000;
}

inline std::string processPackage(std::string name, bool childZygote) {
    name.resize(name.find(':') == std::string::npos ? name.size() : name.find(':'));
    if (childZygote && name.ends_with("_zygote")) name.resize(name.size() - 7);
    bool first = true;
    for (const unsigned char c : name) {
        if (c == '.') {
            if (first) return {};
            first = true;
        } else {
            const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
            if (!letter && (first || c < '0' || c > '9')) return {};
            first = false;
        }
    }
    return first ? std::string() : name;
}

inline std::string isolatedScopeMarker(int uid, const std::string& process, bool childZygote, bool initArt = false) {
    if (!isIsolatedUid(uid) || childZygote) return {};
    // Only support private process names (package:identifier[:instance...]).
    // A global process name is not package identity; its appended component
    // name contains dots and must not select another package's configuration.
    const auto colon = process.find(':');
    if (colon == std::string::npos) return {};
    const auto end = process.find(':', colon + 1);
    const auto suffix = process.substr(colon + 1, end == std::string::npos ? end : end - colon - 1);
    if (suffix.empty() || processPackage(suffix, false) != suffix || suffix.find('.') != std::string::npos) return {};
    const auto package = processPackage(process, childZygote);
    if (package.empty()) return {};
    return std::string("/data/adb/art_fd_rebind/") +
        (initArt ? "init_art_isolated_targets/" : "isolated_targets/") + std::to_string(uid / 100000) + "/" + package;
}

struct ScopeResult {
    int32_t removed = -1;
    int32_t initArt = -1;
};
