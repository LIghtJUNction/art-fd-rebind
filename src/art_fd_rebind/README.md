# App Namespace Experiments

实验性 Zygisk 模块，包含 ART 只读句柄重开、应用启动前的私有 Oplus overlay 隔离。普通应用按 UID 启用；v0.2.1 增加按 Android 用户和包名选择隔离服务。默认不对任何应用启用。

**尚未全部通过 Native Detector 7.7.0。** v0.2.1 本机首次实测中，Oplus 可疑挂载警告已消失，重启后此前的 Magic Mount 再次出现。该版本不宣称解决 Magic Mount。v0.2 的同次开机对照仅确认主应用 Oplus 隔离将可疑挂载从两条降至一条。

## 已验证的结果

在 Android16 / Linux5.10 / KernelSU3.3.1 / ZygiskNext1.5.2 的一台实体设备上：

| 检查 | 结果 |
| --- | --- |
| arm64-v8a、armeabi-v7a 本地构建 | 通过 |
| arm64 独立私有命名空间自测 | 通过 |
| 文件设备号、inode、位置、FD及状态标志保持不变 | 通过 |
| 不改动无关文件句柄；重复执行无额外操作 | 通过 |
| 在应用内将 ART 句柄挂载编号变为当前命名空间编号 | 已观察到，开关对照成立 |
| 通过 ART 句柄重开消除 Native Detector7.7.0 Magic Mount | **未通过** |
| v0.2 Oplus 隔离独立自测及应用挂载视图 | 9个覆盖层移除，系统主命名空间保持原样 |
| v0.2 同次开机冷启动开关对照 | 关闭时2条可疑挂载警告，开启后1条；两种状态均无 Magic Mount |
| v0.2.1 首次装机验证 | 主应用、App Zygote 父进程及9个普通隔离服务分别移除9个覆盖层；Oplus 警告消失，Magic Mount 在本次重启后复现 |
| v0.2.1 隔离服务规则同次开机冷启动对照 | 关闭时 Magic Mount + Oplus 可疑挂载；开启后只剩 Magic Mount |

该结果说明：消除继承句柄与当前命名空间的挂载编号差异，本身不足以消除这项报警。检测器的完整原生判定条件未公开，不能据此认定其具体算法。

## Oplus 隔离实验（v0.2）

使用 NoHello 已有的隔离思路，在 `preAppSpecialize` 中创建新的挂载命名空间、确认身份已变化，并将挂载传播设为递归 PRIVATE，然后才允许卸载其中的 overlay。限定目标是 `/system_ext/framework/oplusex/` 或 `/system_ext/lib64/oplusex/` 下的 overlay 挂载，不处理其他路径。任何隔离前提失败时都不会尝试卸载。

v0.2 当时剩余的警告指向 `/system_ext/lib64/oplusex/com.oplus.NetworkAssistSys`。v0.2 根权限观察到主应用路径已经是 EROFS，但后来发现普通隔离服务仍保留 overlay，主进程线程检查没有覆盖独立服务。v0.2.1 补齐了这部分作用域。社区也有[原厂 Oplus 路径相关报告](https://github.com/reveny/Android-Native-Root-Detector/issues/105)，这不证明本机的具体判定条件。

这是显式启用的应用兼容性实验，可能使目标应用缺少厂商扩展。系统主命名空间及其他应用的挂载不改变。

## 原始 ART 句柄实验（v0.2.4，应用验收待完成）

root companion 只打开固定白名单中的12个公开启动 JAR（5个 ART、7个 system framework；路径见 `native/init_art.hpp`）的 `/proc/1/root` 视图，通过 SCM_RIGHTS 将真实只读句柄交给明确选择的进程。进程在 specialize 前只处理设备号、inode与该文件相同的单个旧句柄，保持原 FD 编号、位置、状态和关闭时标志；遇到多个句柄会跳过，防止耦合独立的读取位置。校验失败会恢复旧句柄；若无法恢复，只终止当前被选择的子进程。

这不改变系统挂载，也不改写 fdinfo。Root 独立自测已观察到原始挂载编号315，文件身份、位置、标志、幂等性、无关FD及多个句柄拒绝处理均通过；v0.2.2 在实机上因额外 O_NOFOLLOW 与原句柄不一致而主动拒绝替换；v0.2.3 按原句柄选择这个标志，同时先后核对固定路径的 regular-file 身份，拒绝末级符号链接与打开期间文件变化。带/不带 O_NOFOLLOW 两种独立自测均通过；v0.2.3 已实际消除 core-libart.jar 的命中，检测器接着显示同组 okhttp.jar；v0.2.4 一次处理上述整组文件，保持每个文件各自的身份、位置和标志。Native 完整验收仍待重启验证。

普通应用用 root-only `/data/adb/art_fd_rebind/init_art_targets/<完整UID>` 标记；符合下文私有进程命名约束的隔离服务用 `/data/adb/art_fd_rebind/init_art_isolated_targets/<Android用户>/<包名>` 标记。目录0700、文件0600、root所有。它与 Oplus 开关独立；若同时选中旧 ART 重开开关，优先使用本实验且不回退旧方式。默认没有目标。

## 隔离服务作用域（v0.2.1）

隔离服务使用临时 UID，可能与主应用拥有不同挂载视图。实机日志已观察到主进程的 Oplus 覆盖层为 0，而普通隔离服务仍有 9 个，NetworkAssistSys 仍为 overlay。v0.2.1 增加按 Android 用户和私有进程名中的包名前缀匹配隔离服务的规则；不使用会被其他应用复用的临时 UID 标记。

例如在 root shell 中启用 Android 用户 0 的 `your.package.name`：

```sh
mkdir -p /data/adb/art_fd_rebind/isolated_targets/0
chmod 700 /data/adb/art_fd_rebind /data/adb/art_fd_rebind/isolated_targets /data/adb/art_fd_rebind/isolated_targets/0
touch /data/adb/art_fd_rebind/isolated_targets/0/your.package.name
chmod 600 /data/adb/art_fd_rebind/isolated_targets/0/your.package.name
```

仅支持 `包名:私有标识符[:实例…]` 形式，私有标识符不能包含点；自定义全局进程名及无法确认这种形式的名称不匹配。App Zygote 父进程仍使用其所属普通 UID 的规则。

这项标记只启用对应隔离服务的 Oplus 功能，不启用 ART 重开。主应用的 UID 标记仍单独设置。移除标记并强制停止主应用可撤销。临时 UID 区间的普通 UID 标记会被忽略。新版首次装机验证：主应用、App Zygote 父进程，以及普通隔离服务均已确认移除 9 个覆盖层；界面不再显示 Oplus 可疑挂载，但仍有 Magic Mount。尚不能据此承诺其他环境兼容性。

可选的 root-only `debug` 标记会将已选择进程的 PID、UID、包名及覆盖层移除数记录到私有 `isolated.log`；默认关闭，不采集用户文件内容。调试结束应删除标记与不再需要的日志。

## ART 句柄工作方式

在 `preAppSpecialize` 中通过标准 Zygisk root companion 查询当前UID是否明确启用；在 `postAppSpecialize` 中处理符合全部条件的文件描述符：

- 路径位于 `/apex/com.android.art/javalib/`，且以 `.jar` 结尾；
- 是只读普通文件，不是 socket 或 O_PATH 描述符；
- 旧挂载编号在当前进程的 mountinfo 中不可见；
- 当前路径与原句柄的设备号、inode相同。

使用 NoHello/AOSP 现有的 `FileDescriptorInfo::ReopenOrDetach`，明确关闭 detach 行为，保留原位置与标志。root授权应用和 system_server 不参与处理。完成后请求卸载模块动态库。

重开文件会建立新的 open file description；目前只做了有限的实体设备实验，未验证其他应用的兼容性。不修改检测器、不伪造检测结果、不修改内核或系统分区。Oplus功能仅在明确启用的目标应用自己的新建私有命名空间中卸载对应覆盖层。

## 构建

需要 Python3、Android NDK、[Kam](https://github.com/MemDeco-WG/Kam)。已在 Linux上使用 NDK30 本地构建，无远程构建或CI依赖。

```sh
python build-native.py --ndk /path/to/android-ndk
kam export prop src/art_fd_rebind/module.prop
kam check
kam build
```

也可设置 `ANDROID_NDK_HOME`；脚本能从 Android SDK 的 `ndk/` 目录发现已安装版本。输出位于 `dist/`。生成的 ZIP 安装流程仅在 KernelSU 验证，其他管理器未验证。

## 启用与撤销

先通过 KernelSU 安装ZIP并重启。安装本身不启用任何目标。查询目标应用 UID（adb shell）：

```sh
cmd package list packages -U your.package.name
```

随后在 root shell 中创建对应UID标记，将 `12345` 替换为查得的UID：

```sh
mkdir -p /data/adb/art_fd_rebind/targets
chmod 700 /data/adb/art_fd_rebind /data/adb/art_fd_rebind/targets
touch /data/adb/art_fd_rebind/targets/12345
chmod 600 /data/adb/art_fd_rebind/targets/12345
```

上面是 ART 句柄开关。若只测试 Oplus 隔离，则不创建 `targets/12345`，改为创建 `/data/adb/art_fd_rebind/oplus_targets/12345`（同样只允许root写入）。两个开关互不依赖。

强制停止并重新打开目标应用。撤销时删除对应标记，再强制停止并重新打开应用。应用卸载或UID变化时也应移除旧标记。完整停用或移除模块通过 KernelSU 操作并重启生效。当前测试设备只对检测应用开启 Oplus 隔离，ART 句柄重开关闭；发行包本身不包含任何目标 UID。

## 独立自测

`build/art-fd-selftest` 是 arm64 Android 可执行文件，需要在设备的可执行临时目录中以root运行。它打开只读 ART JAR，然后仅在自己的私有挂载命名空间中验证重开行为、标志、位置、幂等性，以及无关描述符不变。默认自测不卸载任何路径。传入 `--oplus` 时，在自测进程自己的私有命名空间中卸载匹配的 Oplus overlay，并检查清空及幂等性；外部还应比较系统主命名空间保持不变。两类自测都不代表检测器验收通过。

## 上游与许可

文件描述符工具及 Zygisk API 头文件固定来自 [NoHello c77f10d3](https://github.com/MhmRdd/NoHello/tree/c77f10d3ccfcabbecbb439f24a033d791aeaaf25/module/src/main/cpp)。FD工具原始逻辑来自 AOSP。改动仅包括本地日志头文件路径，以及在 open flags 中保留 O_NOFOLLOW，避免其经 F_SETFL 丢失。

本项目代码采用 Apache-2.0；上游文件保留各自许可和版权头，见 [THIRD_PARTY_NOTICES.md](NOTICE)。原始设备日志、截图、应用列表、配置与其他私人材料未包含在仓库中。

Experimental Zygisk prototype for ART descriptor rebinding and separately opted-in private Oplus overlay isolation. Rebinding works in the tested app, but **does not clear Native Detector7.7.0 Magic Mount**. No targets are enabled by default.
