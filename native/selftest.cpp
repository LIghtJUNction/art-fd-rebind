// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#include <sys/mount.h>
#include <sched.h>
#include <cstdio>
#include "repair.hpp"
#include "oplus.hpp"

int main(int argc, char** argv) {
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
