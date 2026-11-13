// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#pragma once
#include <cstring>
#include <array>
#include <sys/socket.h>
#include "repair.hpp"

// Fixed public boot JAR allowlist; callers cannot choose a privileged pathname.
inline constexpr std::array<const char*, 12> kBootJars = {
    "/apex/com.android.art/javalib/core-libart.jar",
    "/apex/com.android.art/javalib/core-oj.jar",
    "/apex/com.android.art/javalib/okhttp.jar",
    "/apex/com.android.art/javalib/bouncycastle.jar",
    "/apex/com.android.art/javalib/apache-xml.jar",
    "/system/framework/framework.jar",
    "/system/framework/framework-graphics.jar",
    "/system/framework/framework-location.jar",
    "/system/framework/ext.jar",
    "/system/framework/telephony-common.jar",
    "/system/framework/voip-common.jar",
    "/system/framework/ims-common.jar",
};
inline constexpr auto kCoreArtPath = kBootJars[0];

inline bool sendArtSource(int socket, bool noFollow = false, size_t index = 0) {
    if (index >= kBootJars.size()) return false;
    // Fixed public system file only: no client-supplied privileged path.
    const auto path = std::string("/proc/1/root") + kBootJars[index];
    struct stat before{}, st{};
    int fd = -1;
    if (lstat(path.c_str(), &before) == 0 && S_ISREG(before.st_mode) && (before.st_mode & S_IROTH))
        fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | (noFollow ? O_NOFOLLOW : 0));
    if (fd >= 0 && (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || !(st.st_mode & S_IROTH) ||
        before.st_dev != st.st_dev || before.st_ino != st.st_ino)) {
        close(fd); fd = -1;
    }
    unsigned char status = fd >= 0;
    iovec io{&status, sizeof(status)};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{};
    message.msg_iov = &io;
    message.msg_iovlen = 1;
    if (fd >= 0) {
        message.msg_control = control;
        message.msg_controllen = sizeof(control);
        auto* c = CMSG_FIRSTHDR(&message);
        c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(c), &fd, sizeof(fd));
    }
    const bool sent = TEMP_FAILURE_RETRY(sendmsg(socket, &message, MSG_NOSIGNAL)) == 1;
    if (fd >= 0) close(fd);
    return sent;
}

inline int coreArtNoFollow(size_t index = 0) {
    if (index >= kBootJars.size()) return -1;
    struct stat canonical{};
    if (::stat(kBootJars[index], &canonical) != 0) return -1;
    const auto fds = GetOpenFds([](const std::string&) {});
    if (!fds) return -1;
    int result = -1;
    for (const int fd : *fds) {
        struct stat st{};
        if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
            st.st_dev != canonical.st_dev || st.st_ino != canonical.st_ino) continue;
        const int flags = fcntl(fd, F_GETFL);
        if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY || (flags & O_PATH)) continue;
        if (result >= 0) return -1;
        result = (flags & O_NOFOLLOW) != 0;
    }
    return result;
}

inline int receiveArtSource(int socket) {
    unsigned char status = 0;
    iovec io{&status, sizeof(status)};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{};
    message.msg_iov = &io; message.msg_iovlen = 1;
    message.msg_control = control; message.msg_controllen = sizeof(control);
    const auto size = TEMP_FAILURE_RETRY(recvmsg(socket, &message, MSG_CMSG_CLOEXEC));
    bool valid = size == 1 && status == 1 && !(message.msg_flags & (MSG_CTRUNC | MSG_TRUNC));
    int received = -1;
    for (auto* c = CMSG_FIRSTHDR(&message); c; c = CMSG_NXTHDR(&message, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS || c->cmsg_len < CMSG_LEN(sizeof(int))) {
            valid = false; continue;
        }
        const auto count = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (size_t n = 0; n < count; ++n) {
            int next = -1;
            memcpy(&next, CMSG_DATA(c) + n * sizeof(int), sizeof(next));
            if (received < 0) received = next;
            else { close(next); valid = false; }
        }
    }
    if (valid && received >= 0) return received;
    const bool unavailable = size == 1 && status == 0 &&
        !(message.msg_flags & (MSG_CTRUNC | MSG_TRUNC)) && !CMSG_FIRSTHDR(&message);
    if (received >= 0) close(received);
    return unavailable ? -1 : -2; // A broken exchange cannot be reused for another index.
}

inline int rebindInitArtSource(int source, size_t index = 0) {
    if (index >= kBootJars.size()) return -1;
    struct stat origin{};
    if (source < 0 || fstat(source, &origin) != 0 || !S_ISREG(origin.st_mode)) return -1;
    struct stat canonical{};
    if (::stat(kBootJars[index], &canonical) != 0 || canonical.st_dev != origin.st_dev || canonical.st_ino != origin.st_ino) return -1;
    const int sourceFlags = fcntl(source, F_GETFL);
    if (sourceFlags < 0 || (sourceFlags & O_ACCMODE) != O_RDONLY || (sourceFlags & O_PATH)) return -1;
    const auto sourceMount = fdMountId(source);
    if (!sourceMount) return -1;
    const auto fds = GetOpenFds([](const std::string&) {});
    if (!fds) return -1;
    std::unique_ptr<FileDescriptorInfo> target;
    for (const int fd : *fds) {
        if (fd == source) continue;
        auto candidate = FileDescriptorInfo::CreateFromFd(fd, [](const std::string&) {});
        // A pinned FD's displayed path may shorten after its namespace dies.
        // Match the fixed public file by identity, not its rendered pathname.
        if (!candidate || candidate->is_sock || !S_ISREG(candidate->stat.st_mode) ||
            candidate->stat.st_dev != origin.st_dev || candidate->stat.st_ino != origin.st_ino) continue;
        // Do not merge independent file descriptions when multiple handles exist.
        if (target) return -2;
        target = std::move(candidate);
    }
    if (!target) return 0;
    const int fd = target->fd;
    if (!S_ISREG(target->stat.st_mode) || target->stat.st_dev != origin.st_dev ||
        target->stat.st_ino != origin.st_ino || fcntl(fd, F_GETFL) != sourceFlags ||
        (target->fd_flags & ~FD_CLOEXEC) || target->offset < 0) return -3;
    if (fdMountId(fd) == sourceMount) return 0;
    if (lseek64(source, target->offset, SEEK_SET) != target->offset) return -4;
    const int backup = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (backup < 0) return -4;
    const int duplicateFlags = target->fd_flags & FD_CLOEXEC ? O_CLOEXEC : 0;
    if (TEMP_FAILURE_RETRY(dup3(source, fd, duplicateFlags)) < 0) { close(backup); return -4; }
    const bool valid = target->RefersToSameFile() && fdMountId(fd) == sourceMount &&
        fcntl(fd, F_GETFD) == target->fd_flags && fcntl(fd, F_GETFL) == sourceFlags &&
        lseek64(fd, 0, SEEK_CUR) == target->offset;
    if (!valid) {
        const bool restored = TEMP_FAILURE_RETRY(dup3(backup, fd, duplicateFlags)) >= 0;
        if (!restored) {
            LOGE("fatal ART descriptor rollback failure; stopping selected child pid=%d", getpid());
            _exit(125);
        }
        close(backup);
        return -5;
    }
    close(backup);
    return 1;
}
