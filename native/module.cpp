// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include "zygisk.hpp"
#include "repair.hpp"
#include "oplus.hpp"
#include "scope.hpp"
#include "init_art.hpp"

static bool transfer(int fd, void* data, size_t size, bool sending) {
    auto* p = static_cast<char*>(data);
    while (size) {
        const ssize_t n = TEMP_FAILURE_RETRY(sending ? send(fd, p, size, MSG_NOSIGNAL) : recv(fd, p, size, 0));
        if (n <= 0) return false;
        p += n;
        size -= static_cast<size_t>(n);
    }
    return true;
}

static bool scopeMarker(const std::string& path) {
    struct stat st{};
    return !path.empty() && lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
        st.st_uid == 0 && !(st.st_mode & 0022);
}

static void scopeCompanion(int client) {
    timeval timeout{0, 500000};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    ScopeQuery query{};
    unsigned char enabled = 0;
    if (transfer(client, &query, sizeof(query), false) && query.uid >= 10000 &&
        memchr(query.process, 0, sizeof(query.process))) {
        if (isIsolatedUid(query.uid)) {
            // Isolated IDs are recycled: never apply a persistent UID marker to them.
            if (scopeMarker(isolatedScopeMarker(query.uid, query.process, query.childZygote != 0))) enabled = 2;
            if (scopeMarker(isolatedScopeMarker(query.uid, query.process, query.childZygote != 0, true))) enabled |= 4;
        } else {
            enabled = scopeMarker("/data/adb/art_fd_rebind/targets/" + std::to_string(query.uid));
            if (scopeMarker("/data/adb/art_fd_rebind/oplus_targets/" + std::to_string(query.uid))) enabled |= 2;
            if (scopeMarker("/data/adb/art_fd_rebind/init_art_targets/" + std::to_string(query.uid))) enabled |= 4;
        }
    }
    if (transfer(client, &enabled, sizeof(enabled), true) && (enabled & 6)) {
        if (enabled & 4) {
            unsigned char noFollow = 0;
            if (!transfer(client, &noFollow, sizeof(noFollow), false) || noFollow > 1 ||
                !sendArtSource(client, noFollow != 0)) { close(client); return; }
        }
        ScopeResult result{};
        if (transfer(client, &result, sizeof(result), false) &&
            scopeMarker("/data/adb/art_fd_rebind/debug")) {
            const int log = open("/data/adb/art_fd_rebind/isolated.log",
                O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (log >= 0) {
                const auto package = processPackage(query.process, query.childZygote != 0);
                dprintf(log, "pid=%d uid=%d child_zygote=%d policy=%u removed=%d init_art=%d package=%s\n",
                    query.pid, query.uid, query.childZygote != 0, enabled, result.removed, result.initArt, package.c_str());
                close(log);
            }
        }
    }
    close(client);
}

class ArtFdRebind final : public zygisk::ModuleBase {
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    unsigned char enabled = 0;
public:
    void onLoad(zygisk::Api* value, JNIEnv* jni) override { api = value; env = jni; }
    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        enabled = 0;
        if (args->uid >= 10000 && !(api->getFlags() & zygisk::StateFlag::PROCESS_GRANTED_ROOT)) {
            const int client = api->connectCompanion();
            if (client >= 0) {
                timeval timeout{0, 500000};
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
                ScopeQuery query{};
                query.uid = args->uid;
                query.pid = getpid();
                query.childZygote = args->is_child_zygote && *args->is_child_zygote;
                if (args->nice_name && static_cast<size_t>(env->GetStringUTFLength(args->nice_name)) < sizeof(query.process)) {
                    env->GetStringUTFRegion(args->nice_name, 0, env->GetStringLength(args->nice_name), query.process);
                }
                unsigned char policy = 0;
                if (transfer(client, &query, sizeof(query), true) &&
                    transfer(client, &policy, sizeof(policy), false)) enabled = policy;
                ScopeResult result{};
                int source = -1;
                if (enabled & 4) {
                    unsigned char noFollow = coreArtNoFollow() == 1;
                    transfer(client, &noFollow, sizeof(noFollow), true);
                    source = receiveArtSource(client);
                }
                if (enabled & 2) result.removed = isolateOplusOverlays();
                if (enabled & 4) {
                    result.initArt = rebindInitArtSource(source);
                    if (source >= 0) close(source);
                }
                if (enabled & 6) transfer(client, &result, sizeof(result), true);
                close(client);
                if (enabled) LOGI("selected v0.2.3 pid=%d uid=%d policy=%u", getpid(), args->uid, enabled);
            }
        }
        if (!enabled) api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const zygisk::AppSpecializeArgs*) override {
        if ((enabled & 1) && !(enabled & 4)) {
            const int count = repairStaleArtFds();
            LOGI("completed pid=%d uid=%d repaired=%d", getpid(), getuid(), count);
        }
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
    void preServerSpecialize(zygisk::ServerSpecializeArgs*) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
};
REGISTER_ZYGISK_MODULE(ArtFdRebind)
REGISTER_ZYGISK_COMPANION(scopeCompanion)
