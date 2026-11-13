// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include "fdutils/fd_utils.h"
#include "log.h"

inline unsigned long fdMountId(int fd) {
    std::ifstream input("/proc/self/fdinfo/" + std::to_string(fd));
    std::string line;
    while (std::getline(input, line)) {
        if (line.starts_with("mnt_id:")) {
            unsigned long id = 0;
            std::istringstream(line.substr(7)) >> id;
            return id;
        }
    }
    return 0;
}

inline std::set<unsigned long> visibleMountIds() {
    std::set<unsigned long> ids;
    std::ifstream input("/proc/self/mountinfo");
    std::string line;
    while (std::getline(input, line)) {
        unsigned long id = 0;
        if (std::istringstream(line) >> id) ids.insert(id);
    }
    return ids;
}

inline int repairStaleArtFds() {
    const auto mounts = visibleMountIds();
    if (mounts.empty()) return 0;
    auto ignore = [](const std::string&) {};
    const auto fds = GetOpenFds(ignore);
    if (!fds) return 0;
    int repaired = 0;
    for (int fd : *fds) {
        const auto info = FileDescriptorInfo::CreateFromFd(fd, ignore);
        if (!info || info->is_sock || !S_ISREG(info->stat.st_mode)) continue;
        if (!info->file_path.starts_with("/apex/com.android.art/javalib/") ||
            !info->file_path.ends_with(".jar")) continue;
        const int flags = fcntl(fd, F_GETFL);
        if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY || (flags & O_PATH)) continue;
        const auto oldId = fdMountId(fd);
        if (!oldId || mounts.contains(oldId)) continue;
        struct stat current{};
        if (::stat(info->file_path.c_str(), &current) != 0 ||
            current.st_dev != info->stat.st_dev || current.st_ino != info->stat.st_ino) continue;
        bool failed = false;
        info->ReopenOrDetach([&](const std::string& error) {
            failed = true;
            LOGE("reopen failed: %s", error.c_str());
        }, false);
        if (failed) continue;
        const auto newId = fdMountId(fd);
        if (!info->RefersToSameFile() || !mounts.contains(newId) ||
            fcntl(fd, F_GETFD) != info->fd_flags ||
            fcntl(fd, F_GETFL) != flags ||
            lseek64(fd, 0, SEEK_CUR) != info->offset) {
            LOGE("post-reopen invariant failed fd=%d old=%lu new=%lu", fd, oldId, newId);
            continue;
        }
        ++repaired;
        LOGI("rebound pid=%d fd=%d old_mnt=%lu new_mnt=%lu path=%s",
             getpid(), fd, oldId, newId, info->file_path.c_str());
    }
    return repaired;
}
