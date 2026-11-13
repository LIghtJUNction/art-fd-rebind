# v0.2.0 — experimental

- Add independent UID opt-in for pre-specialization Oplus overlay isolation.
- Require a new mount namespace and recursive PRIVATE propagation before any detach.
- Limit matching to overlay mounts under system_ext framework/lib64 oplusex directories.
- ART rebinding remains a separate switch; it does not clear Native Detector7.7 Magic Mount.
- Private-namespace device self-test removed9overlays with init mounts byte-identical. App acceptance is pending a coordinated reboot.

# v0.1.1 — experimental

- First public prototype, using pinned NoHello/AOSP FD utilities.
- UID opt-in through a Zygisk root companion.
- Read-only ART JAR descriptor rebinding with device/inode, flag, and offset checks.
- Local arm64/armv7 builds and arm64 private-namespace self-test passed.
- App A/B test confirmed descriptor rebinding, but Native Detector7.7 Magic Mount remained. This release is not a successful detector bypass.
