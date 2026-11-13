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
