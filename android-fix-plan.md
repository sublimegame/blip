# Plan: Fix Android Compilation

## Context

The repository was recently restructured (PR #947 `repo-restructure`), moving the Android project from the repo root to `clients/android/`. Several path references in build files were updated in commit `6d7d339d`, but remaining issues prevent successful compilation. The current branch is `fix-android-compilation`.

## Issues Found (in order of build failure)

### Issue 1: Missing `local.properties` (BLOCKING)
- **File:** `clients/android/local.properties` (does not exist)
- **Problem:** The old `local.properties` was at root `/android/local.properties` and didn't get carried over. Without it, Gradle can't find the Android SDK.
- **Fix:** Create `clients/android/local.properties` with `sdk.dir=/Users/gaetan/Library/Android/sdk`
- **Note:** This file is gitignored and per-developer, so it just needs to exist locally.

### Issue 2: Missing `google-services.json` (BLOCKING)
- **File:** `clients/android/app/google-services.json` (does not exist, only `.template`)
- **Problem:** The `com.google.gms.google-services` Gradle plugin fails if this file is missing. The real file contains Firebase project credentials and is not committed to the repo.
- **Fix:** Copy the template as a placeholder for local dev builds: `cp google-services.json.template google-services.json` in `clients/android/app/`. For production builds, the real credentials file needs to be provided separately.

### Issue 3: `.env` file path is wrong
- **File:** `clients/android/app/build.gradle` line 16
- **Current:** `def envFile = rootProject.file("../.env")` → resolves to `clients/.env`
- **Should be:** `def envFile = rootProject.file("../../.env")` → resolves to repo root `.env`
- **Impact:** Environment variables (API keys etc.) won't be forwarded to CMake. Build won't fail but the app will lack config values.

### Issue 4: Keystore paths use Windows backslash separators
- **File:** `clients/android/app/build.gradle` lines 31, 37
- **Current:**
  - Line 31: `storeFile file('.\\..\\..\\pcubes-keystore.jks')`
  - Line 37: `storeFile file('.\\..\\voxowl_debug.jks')`
- **Fix:**
  - Line 31: `storeFile file('../../pcubes-keystore.jks')`
  - Line 37: `storeFile file('../voxowl_debug.jks')`
- **Impact:** Debug/release signing fails on macOS/Linux.

### Issue 5: Stale root `android/` directory
- **Problem:** Old build artifacts remain at the repo root in an untracked `android/` directory (contains `.gradle/`, `.idea/`, `build/` caches). This is confusing and could mislead IDE project detection.
- **Fix:** Delete the root `android/` directory.

## Files to Modify

1. **`clients/android/local.properties`** — create (not committed, gitignored)
2. **`clients/android/app/google-services.json`** — copy from template (not committed, gitignored)
3. **`clients/android/app/build.gradle`** — fix `.env` path (line 16) and keystore paths (lines 31, 37)
4. **Root `android/`** — delete stale directory

## Verification

1. Run `cd clients/android && ./gradlew assembleDebug` and confirm the build completes
2. If C++ compilation issues surface after the above fixes, address them incrementally
3. Verify the APK is generated at `clients/android/app/build/outputs/apk/debug/`
