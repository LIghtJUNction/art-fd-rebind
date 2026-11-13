// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 LIghtJUNction
#pragma once
#include <android/log.h>
#define LOGD(...) ((void)0)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"ARTFdRebind",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"ARTFdRebind",__VA_ARGS__)
