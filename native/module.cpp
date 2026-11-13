// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#include <sys/types.h>
#include <sys/socket.h>
#include "zygisk.hpp"
#include "repair.hpp"

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

static void scopeCompanion(int client) {
    int uid = -1;
    unsigned char enabled = 0;
    if (transfer(client, &uid, sizeof(uid), false) && uid >= 10000) {
        const auto file = "/data/adb/art_fd_rebind/targets/" + std::to_string(uid);
        struct stat st{};
        enabled = ::stat(file.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
    transfer(client, &enabled, sizeof(enabled), true);
    close(client);
}

class ArtFdRebind final : public zygisk::ModuleBase {
    zygisk::Api* api = nullptr;
    bool enabled = false;
public:
    void onLoad(zygisk::Api* value, JNIEnv*) override { api = value; }
    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (args->uid >= 10000 && !(api->getFlags() & zygisk::StateFlag::PROCESS_GRANTED_ROOT)) {
            const int client = api->connectCompanion();
            if (client >= 0) {
                timeval timeout{0, 500000};
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
                int uid = args->uid;
                unsigned char policy = 0;
                enabled = transfer(client, &uid, sizeof(uid), true) &&
                    transfer(client, &policy, sizeof(policy), false) && policy;
                close(client);
                if (enabled) LOGI("selected v0.1.1 pid=%d uid=%d", getpid(), uid);
            }
        }
        if (!enabled) api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const zygisk::AppSpecializeArgs*) override {
        if (enabled) {
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
