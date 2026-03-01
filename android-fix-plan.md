# Plan: Fix Android Compilation

## Context

The repository was recently restructured (PR #947 `repo-restructure`), moving the Android project from the repo root to `clients/android/`. Several path references in build files were updated in commit `6d7d339d`, but remaining issues prevent successful compilation. The current branch is `fix-android-compilation`.

## Phase 1: Fix build configuration

- [x] **1a.** Create `clients/android/local.properties` with `sdk.dir` — already exists locally.
- [x] **1b.** Copy `google-services.json.template` → `google-services.json` in `clients/android/app/` (gitignored, local dev placeholder).
- [x] **1c.** Fix `.env` path in `clients/android/app/build.gradle` line 16: `../.env` → `../../.env`.
- [x] **1d.** Fix keystore paths in `clients/android/app/build.gradle` lines 31, 37: replaced Windows backslash separators with forward slashes.

## Phase 2: Clean up stale root directory

- [x] **2a.** Delete the root `android/` directory (old build artifacts: `.gradle/`, `.idea/`, `build/` caches). Removed 936K of stale files.

## Phase 3: Verification

- [x] **3a.** Run `cd clients/android && ./gradlew assembleDebug` — initial run revealed CMake path errors: `REPO_ROOT_DIR` in `CMakeLists.txt` and `xptools.cmake` used `CMAKE_CURRENT_BINARY_DIR` which resolved incorrectly after the repo restructure. Fixed both to use `CMAKE_CURRENT_SOURCE_DIR`. Build then completed successfully.
- [x] **3b.** APK generated at `clients/android/app/build/outputs/apk/debug/app-debug.apk` (~121 MB).

## Files to Modify

1. **`clients/android/local.properties`** — create (not committed, gitignored) ✅
2. **`clients/android/app/google-services.json`** — copy from template (not committed, gitignored)
3. **`clients/android/app/build.gradle`** — fix `.env` path (line 16) and keystore paths (lines 31, 37)
4. **Root `android/`** — delete stale directory
