// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#include <sys/mount.h>
#include <sched.h>
#include <cstdio>
#include "repair.hpp"
#include "oplus.hpp"
#include "init_art.hpp"

int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--init-art" || std::string(argv[1]) == "--init-art-plain")) {
        const bool noFollow = std::string(argv[1]) == "--init-art";
        for (int standard = 0; standard <= 2; ++standard) {
            if (fcntl(standard, F_GETFD) < 0 && open("/dev/null", O_RDWR) != standard) return 28;
        }
        if (unshare(CLONE_NEWNS) != 0 || mount(nullptr, "/", nullptr, MS_PRIVATE | MS_REC, nullptr) != 0) return 29;
        const int art = open(kCoreArtPath, O_RDONLY | O_CLOEXEC | (noFollow ? O_NOFOLLOW : 0));
        const int other = open("/system/etc/hosts", O_RDONLY | O_CLOEXEC);
        if (art < 0 || other < 0 || lseek64(art, 4, SEEK_SET) != 4) return 30;
        const auto old = fdMountId(art), otherId = fdMountId(other);
        const auto info = FileDescriptorInfo::CreateFromFd(art, [](const std::string&) {});
        if (info) printf("target_fd=%d path=%s\n", art, info->file_path.c_str());
        if (!info || unshare(CLONE_NEWNS) != 0 || mount(nullptr, "/", nullptr, MS_PRIVATE | MS_REC, nullptr) != 0) return 31;
        int sockets[2];
        if (coreArtNoFollow() != static_cast<int>(noFollow) ||
            socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) return 32;
        unsigned char requested = coreArtNoFollow() == 1, received = 2;
        if (TEMP_FAILURE_RETRY(send(sockets[1], &requested, 1, MSG_NOSIGNAL)) != 1 ||
            TEMP_FAILURE_RETRY(recv(sockets[0], &received, 1, 0)) != 1 || received > 1 ||
            !sendArtSource(sockets[0], received != 0)) return 32;
        const int source = receiveArtSource(sockets[1]);
        const auto origin = fdMountId(source);
        const int flags = fcntl(art, F_GETFL);
        const int repaired = rebindInitArtSource(source);
        bool pass = repaired == 1 && origin && origin != old && fdMountId(art) == origin &&
            info->RefersToSameFile() && fcntl(art, F_GETFL) == flags && fcntl(art, F_GETFD) == info->fd_flags &&
            lseek64(art, 0, SEEK_CUR) == 4 && fdMountId(other) == otherId && rebindInitArtSource(source) == 0;
        const int extra = open(kCoreArtPath, O_RDONLY | O_CLOEXEC | (noFollow ? O_NOFOLLOW : 0));
        const auto extraMount = fdMountId(extra);
        const bool multipleRefused = extra >= 0 && rebindInitArtSource(source) == -2 &&
            fdMountId(art) == origin && fdMountId(extra) == extraMount && lseek64(art, 0, SEEK_CUR) == 4 &&
            lseek64(extra, 0, SEEK_CUR) == 0;
        pass = pass && multipleRefused;
        printf("old=%lu init=%lu repaired=%d invariants=%d unrelated_fd_unchanged=%d multiple_handles_refused=%d\n", old, origin, repaired, pass, fdMountId(other) == otherId, multipleRefused);
        if (extra >= 0) close(extra);
        close(source); close(art); close(other); close(sockets[0]); close(sockets[1]);
        return pass ? 0 : 33;
    }
    if (argc == 2 && std::string(argv[1]) == "--oplus") {
        const auto before = oplusOverlayTargets().size();
        const auto ns = mountNamespaceIdentity();
        const int removed = isolateOplusOverlays();
        const auto after = oplusOverlayTargets().size();
        const bool privateNs = mountNamespaceIdentity() != ns;
        const bool repeatNoop = isolateOplusOverlays() == 0;
        std::printf("before=%zu removed=%d after=%zu new_namespace=%d repeat_noop=%d\n",
            before, removed, after, privateNs, repeatNoop);
        return before > 0 && removed == static_cast<int>(before) && after == 0 && privateNs && repeatNoop ? 0 : 20;
    }
    const int art = open("/apex/com.android.art/javalib/core-libart.jar", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    const int other = open("/system/etc/hosts", O_RDONLY | O_CLOEXEC);
    if (art < 0 || other < 0 || lseek64(art, 4, SEEK_SET) != 4) return 10;
    const auto oldId = fdMountId(art);
    if (!visibleMountIds().contains(oldId) || repairStaleArtFds() != 0) return 11;
    if (unshare(CLONE_NEWNS) != 0 || mount(nullptr, "/", nullptr, MS_PRIVATE | MS_REC, nullptr) != 0) return 12;
    const auto otherId = fdMountId(other);
    if (visibleMountIds().contains(oldId)) return 13;
    const auto info = FileDescriptorInfo::CreateFromFd(art, [](const std::string&) {});
    if (!info) return 14;
    const int originalFlags = fcntl(art, F_GETFL);
    const int count = repairStaleArtFds();
    const auto newId = fdMountId(art);
    const bool valid = count == 1 && newId != oldId && visibleMountIds().contains(newId) &&
        info->RefersToSameFile() && lseek64(art, 0, SEEK_CUR) == 4 &&
        fcntl(art, F_GETFD) == info->fd_flags && fcntl(art, F_GETFL) == originalFlags &&
        fdMountId(other) == otherId && repairStaleArtFds() == 0;
    std::printf("old=%lu new=%lu repaired=%d offset=%lld flags=%d invariant_pass=%d unrelated_fd_unchanged=%d\n",
        oldId, newId, count, (long long)lseek64(art, 0, SEEK_CUR), fcntl(art, F_GETFL), valid, fdMountId(other) == otherId);
    close(art); close(other);
    return valid ? 0 : 15;
}
