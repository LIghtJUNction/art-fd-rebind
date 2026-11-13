# App Namespace Experiments

实验性 Zygisk 模块，包含两个独立的UID开关：ART只读句柄重开，以及应用启动前的私有 Oplus overlay 隔离。默认不对任何应用启用。

**当前不能消除 Native Detector 7.7.0 的 Magic Mount 报警。** 本项目保留可复现的实验代码，用于研究文件描述符与挂载命名空间的关系，不宣称通过 root 检测。

## 已验证的结果

在 Android16 / Linux5.10 / KernelSU3.3.1 / ZygiskNext1.5.2 的一台实体设备上：

| 检查 | 结果 |
| --- | --- |
| arm64-v8a、armeabi-v7a 本地构建 | 通过 |
| arm64 独立私有命名空间自测 | 通过 |
| 文件设备号、inode、位置、FD及状态标志保持不变 | 通过 |
| 不改动无关文件句柄；重复执行无额外操作 | 通过 |
| 在应用内将 ART 句柄挂载编号变为当前命名空间编号 | 已观察到，开关对照成立 |
| 消除 Native Detector7.7.0 Magic Mount | **未通过** |
| v0.2 Oplus 隔离独立自测 | 9个覆盖层移除，系统主命名空间保持原样；应用检测结果待验证 |

该结果说明：消除继承句柄与当前命名空间的挂载编号差异，本身不足以消除这项报警。检测器的完整原生判定条件未公开，不能据此认定其具体算法。

## Oplus 隔离实验（v0.2）

使用 NoHello 已有的隔离思路，在 `preAppSpecialize` 中创建新的挂载命名空间、确认身份已变化，并将挂载传播设为递归 PRIVATE，然后才允许卸载其中的 overlay。限定目标是 `/system_ext/framework/oplusex/` 或 `/system_ext/lib64/oplusex/` 下的 overlay 挂载，不处理其他路径。任何隔离前提失败时都不会尝试卸载。

这是显式启用的应用兼容性实验，可能使目标应用缺少厂商扩展。系统主命名空间及其他应用的挂载不改变。

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

强制停止并重新打开目标应用。撤销时删除对应标记，再强制停止并重新打开应用。应用卸载或UID变化时也应移除旧标记。完整停用或移除模块通过 KernelSU 操作并重启生效。测试设备上的标记已移除，模块已停用。

## 独立自测

`build/art-fd-selftest` 是 arm64 Android 可执行文件，需要在设备的可执行临时目录中以root运行。它打开只读 ART JAR，然后仅在自己的私有挂载命名空间中验证重开行为、标志、位置、幂等性，以及无关描述符不变。默认自测不卸载任何路径。传入 `--oplus` 时，在自测进程自己的私有命名空间中卸载匹配的 Oplus overlay，并检查清空及幂等性；外部还应比较系统主命名空间保持不变。两类自测都不代表检测器验收通过。

## 上游与许可

文件描述符工具及 Zygisk API 头文件固定来自 [NoHello c77f10d3](https://github.com/MhmRdd/NoHello/tree/c77f10d3ccfcabbecbb439f24a033d791aeaaf25/module/src/main/cpp)。FD工具原始逻辑来自 AOSP。改动仅包括本地日志头文件路径，以及在 open flags 中保留 O_NOFOLLOW，避免其经 F_SETFL 丢失。

本项目代码采用 Apache-2.0；上游文件保留各自许可和版权头，见 [THIRD_PARTY_NOTICES.md](NOTICE)。原始设备日志、截图、应用列表、配置与其他私人材料未包含在仓库中。

Experimental Zygisk prototype for ART descriptor rebinding and separately opted-in private Oplus overlay isolation. Rebinding works in the tested app, but **does not clear Native Detector7.7.0 Magic Mount**. No targets are enabled by default.
