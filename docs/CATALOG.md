# Catalog submission preparation

Do not submit this prototype as a live standalone BLE detector. The requested stock/Full native scanning capability is unavailable through the inspected official API. Catalog description must say stock standalone capture inspection, optional Wi-Fi Beacon live reception with companion.

Completed: external-app manifest, GPIO category, 10x10 monochrome icon, version 0.1, source/license attribution, reproducible build scripts, official SDK exported-symbol check.

Outstanding:

1. Complete hardware checks in HARDWARE.md and capture qFlipper screenshots.
2. Pin a reviewed commit from https://github.com/mobius2016/DroneRID for submission.
3. Prepare Catalog `applications/GPIO/drone_rid/manifest.yml` using that real origin/commit, `location.subdir: flipper`, screenshot paths, description/changelog.
4. Because Catalog includes resolve relative to the source subdirectory, copy the approved README, changelog and screenshot assets into `flipper` or select the correct supported paths before validation. Do not assume parent-directory includes are accepted.
5. Use the Catalog repository's manifest validator and resolve findings, then submit for review. Catalog approval is separate from FAP compilation.

The source repository is https://github.com/mobius2016/DroneRID. Genuine screenshots, a reviewed release commit and manifest validation remain required. No PR or Catalog submission has been created.
