# CI and release contract

[Repository checks](../.github/workflows/checks.yml) run actionlint 1.7.12 (download verified by SHA-256), static validation and Python tests, including on documentation changes. [Firmware workflow](../.github/workflows/firmware.yml) runs only for source/config/tooling changes, manual dispatch, or version tags. Documentation-only branch changes do not trigger full builds.

The discover job accepts `all`, a direct example name, or a repository-relative path. Tags always select `all`. The matrix is **12 first-party projects × 1 approved experimental framework lane = 12 required builds / 12 firmware ZIPs**. The lane is `6.2.0-dev-08e0d30a74ad`, pinned to `08e0d30a74ad0bfd5a34933142b80f45619ee410`, with `experimental: true`. It is required for release completeness, not an optional job. Local compilation and archive results are recorded in [validation](validation.md); GitHub execution is a separate acceptance check.

Every approved lane installs ESP-IDF from its exact commit with recursive submodules, installs that revision's tools, pins Component Manager 3.1.2 and runs the common build entry. Jobs are marked with `experimental` status. No failed or experimental job is silently optional. Successful branch jobs upload Actions artifacts; a partly successful branch may have artifacts, but cannot publish a partial Release.

Builds run at most four matrix jobs concurrently, with a 60-minute timeout per job. Build and package steps report failures separately. Failed builds retain the build log, CMake diagnostics and resolved dependency lock as `diagnostics-*` artifacts for seven days. These diagnostics are excluded from firmware downloads and Release assets.

Every successful source build run, including pull requests, downloads its `firmware-*` artifacts and validates the complete selected set against that checkout's commit. This catches missing archives and packaging mismatches before a release tag is created. A manual single-example build validates only its selected matrix; version tags and the publisher independently require all 12 packages.

## Release gates

1. Only a push of strict `vMAJOR.MINOR.PATCH` may publish. The broad trigger is narrowed by strict semantic-version validation.
2. Static checks, **every** build matrix job and the artifact-set validation job must succeed. No `always()` or `continue-on-error` bypass exists.
3. Check local tag, event commit, checkout, fresh remote tag and default-branch ancestry.
4. Independently validate every ZIP, exact required set, full source commit, target, framework revision and filename. Reject missing, duplicate, extra, padded, corrupt or stale packages.
5. Refuse an existing Release, including an existing draft. A failed draft requires an explicit maintainer review/cleanup before retry; public versions are never overwritten.
6. Create a draft, upload only the validated set, and compare all remote asset names, sizes, upload states and SHA-256 digests.
7. Publish only the verified draft. Re-read the public Release and freshly download/validate a representative ZIP.

The Release job alone gets `contents: write`. Checkout credentials are not persisted. Actions are pinned by full commit and tracked by Dependabot. Same-ref runs are serialized without cancelling an in-progress publication.

## Pull request preflight

Use a clean source checkout and the activated pinned ESP-IDF environment, with Component Manager 3.1.2 installed. Existing local `sdkconfig` files must be preserved outside that checkout. Ubuntu Bash:

```bash
set -euo pipefail
version=6.2.0-dev-08e0d30a74ad
python3 -m unittest discover -s tests -v
python3 scripts/validate_repo.py
python scripts/build_example.py all --version "$version"
for project in examples/esp-idf/*; do
  test -f "$project/CMakeLists.txt" || continue
  python releases/package_firmware.py --example "${project##*/}" --version "$version"
done
python3 releases/validate_release.py releases/dist --commit "$(git rev-parse HEAD)"
git diff --check
```

The packager requires a clean tracked source tree and refuses existing archive filenames. Use a fresh ignored output directory such as `releases/downloads/preflight` via `--output` for a repeat packaging run and validate that same directory. Verify that the GitHub Actions run for the reviewed commit has passed both builds and `Validate firmware artifact set` before creating a release tag.

## Hosting prerequisites

The supplied checkout currently has one primary remote; no GitHub publication repository, mirror direction or tag-protection configuration was verified. These are deployment prerequisites, not assumptions built into source code. GitHub Actions must be enabled in the chosen GitHub repository. Mirror or explicitly synchronize the reviewed source and tags only after separate authorization. Use a public GitHub repository for the unauthenticated download and public post-publication verification paths. Protect default branches and version tags. No remote settings were changed by this audit.
