# v0.2.4 — experimental (app acceptance pending)

- Rebind the 12 fixed public boot JARs inherited at FD8–19 on the tested device, rather than stopping after core-libart.jar. No client-selected pathname or arbitrary file proxy.
- Gather real source descriptors before namespace isolation, preserving independent per-file identities, flags and positions; retain rollback and multiple-handle refusal.
- v0.2.3 app logs confirmed genuine core-libart replacement, then Native Detector displayed okhttp.jar as its next first match. Full v0.2.4 app acceptance remains pending.

# v0.2.3 — experimental (core-libart repaired; other inherited JARs remain)

- Preserve the original O_NOFOLLOW setting instead of adding it to every source descriptor.
- The root companion still opens only the fixed public ART path; reject a symlink before opening and compare pre/post open device/inode and regular/world-readable status.
- Independent device tests pass both original-flag variants, including the plain open used by the actual app. No target descriptor was changed in the rejected v0.2.2 app run.

# v0.2.2 — experimental (guard rejected on device)

- Independently opted-in original-init ART descriptor source, provided by the root companion over SCM_RIGHTS.
- Fixed public core-libart.jar path only, no caller-selected privileged filename; verify identity, read-only status, flags and position before replacement.
- Preserve the target FD number; refuse multiple matching handles; rollback validation failures and stop only the selected child if rollback is unrecoverable.
- Independent private-namespace device self-test passed: genuine source mount315, device/inode/position/flags intact, idempotent, unrelated FD unchanged, multiple handles refused.
- Device run rejected the replacement because the extra O_NOFOLLOW differed from the original flags; no descriptor was changed.

# v0.2.1 — experimental

- Select isolated services by Android user and package name, including dynamic private service-instance process names. Reject ambiguous global names; app-zygote parents retain their ordinary UID policy.
- Ignore persistent numeric UID markers for recycled isolated UIDs; ART rebinding remains disabled for these services.
- Reset inherited policy before every specialization; validate root-owned marker files and bound companion requests.
- Optional private audit of selected processes and overlay removal counts.
- Root-side live capture confirmed the previous gap: main app has zero targeted overlays while normal isolated services retain all nine. First v0.2.1 device run confirms removal in main, app-zygote parent, and all nine normal isolated services. The Oplus warning is absent, but Magic Mount returned after reboot; this is not a complete detector pass.
- Same-boot isolated-rule cold-start A/B: off gives Magic Mount plus Oplus Suspicious Mount; on leaves only Magic Mount.

# v0.2.0 — experimental

- Add independent UID opt-in for pre-specialization Oplus overlay isolation.
- Require a new mount namespace and recursive PRIVATE propagation before any detach.
- Limit matching to overlay mounts under system_ext framework/lib64 oplusex directories.
- ART rebinding remains a separate switch; it does not clear Native Detector7.7 Magic Mount.
- Private-namespace device self-test removed9overlays with init mounts byte-identical; the target app mount view also no longer contains them.
- Same-boot cold-start A/B: Oplus off gives two suspicious-mount warnings, on leaves one NetworkAssistSys warning. Neither state reports Magic Mount in this boot; this feature is not credited with clearing Magic Mount.

# v0.1.1 — experimental

- First public prototype, using pinned NoHello/AOSP FD utilities.
- UID opt-in through a Zygisk root companion.
- Read-only ART JAR descriptor rebinding with device/inode, flag, and offset checks.
- Local arm64/armv7 builds and arm64 private-namespace self-test passed.
- App A/B test confirmed descriptor rebinding, but Native Detector7.7 Magic Mount remained. This release is not a successful detector bypass.
