# Repository Restructure & Build Fix Plan

Reorganize the repository by moving content out of `private-repo-import/`, creating a clean project structure, and fixing builds for all platforms.

## Context

- The private repo (`voxowl/particubes-private`) was the main repo; the public repo (`bliporg/blip`) was mounted as a git submodule at `cubzh/` inside it
- After merging, root-level directories (`core/`, `deps/`, `shaders/`, etc.) are the former public submodule content
- All build files inside `private-repo-import/` reference `cubzh/core/`, `cubzh/deps/` etc. via relative paths — these are now broken
- Project has been renamed from "cubzh" to "blip"
- On `origin/main`, the private repo was imported as `private-repo-import/` (flat copy, no history)

## Previous attempt (preserved as `repo-restructure-old`)

Phases 1-3 were completed on a local `main` that merged private repo with full git history. This created an incompatible history with `origin/main` (no common ancestor), making the branch unpushable (6+ GiB pack vs 2GB GitHub limit). The old branch is preserved as `repo-restructure-old` for reference — all the path fixes and build fixes documented there are being replayed here.

## Target Structure

```
blip/
├── core/                    # C/C++ engine core (already at root)
├── common/                  # Shared C++ modules
│   ├── VXFramework/         #   Game engine framework
│   ├── VXLuaSandbox/        #   Lua scripting sandbox
│   ├── VXNetworking/        #   Networking layer
│   ├── VXGameServer/        #   Game server logic
│   └── assets/              #   Shared assets
├── clients/                 # Platform-specific client code
│   ├── ios-macos/           #   Xcode workspace (iOS + macOS)
│   ├── android/             #   Gradle project
│   ├── windows/             #   Visual Studio project
│   └── wasm/                #   WebAssembly (kept for future)
├── servers/                 # Backend services & infrastructure
├── go/                      # Go backend source code
├── deps/                    # All external dependencies (merged)
├── shaders/                 # BGFX shaders
├── lua/                     # Lua modules & API docs
├── bundle/                  # Game bundle assets
├── mods/                    # Game mods
├── i18n/                    # Internationalization
├── cli/                     # CLI tools
├── ci/                      # CI/CD infrastructure (merged)
├── dagger/                  # Dagger pipeline
├── dockerfiles/             # Docker build configs (merged)
├── distribution/            # Release & distribution management
├── misc/                    # Miscellaneous assets (merged)
├── scripts/                 # Build & deployment scripts (new)
├── plans/                   # Planning documentation
├── BUILD.md
├── README.md
└── ...
```

## Phase 1: Move directories out of private-repo-import

Move all content from `private-repo-import/` into its target location at the root. No path fixes yet — just the moves. Each subtask is one git mv + commit.

- [x] 1a: Move `private-repo-import/common/` → `common/`
- [x] 1b: Move `private-repo-import/ios-macos/` → `clients/ios-macos/`
- [x] 1c: Move `private-repo-import/android/` → `clients/android/`
- [x] 1d: Move `private-repo-import/windows/` → `clients/windows/`
- [x] 1e: Move `private-repo-import/wasm/` → `clients/wasm/`
- [x] 1f: Move `private-repo-import/servers/` → `servers/` (no existing servers/ on origin/main)
- [x] 1g: Move `private-repo-import/go/` → `go/`
- [x] 1h: Merge `private-repo-import/deps/` into `deps/` (freetype, harfbuzz, gif_load, hasher, pthread — no conflicts with existing deps)
- [x] 1i: Merge `private-repo-import/ci/` into `ci/` (no overlaps with existing lua_docs_deploy_prod)
- [x] 1j: Merge `private-repo-import/dockerfiles/` into `dockerfiles/` (no overlaps with existing luadev.Dockerfile)
- [x] 1k: Move `private-repo-import/distribution/` → `distribution/`
- [x] 1l: Merge `private-repo-import/misc/` into `misc/`, move `private-repo-import/graphic-assets/` → `misc/graphic-assets/`
- [x] 1m: Handle remaining files. Moved: .clang-format→root, scripts (secure-modules.sh, convert_env_to_xcconfig.sh, convert_env_to_header.*)→scripts/. Merged .gitattributes and .gitignore into root. Removed: .dockerignore, .env.example (dup), README.md
- [x] 1n: Remove `private-repo-import/` directory (all content moved, only empty dirs remained)

## Phase 2: Fix path references across all build systems

After all directories are moved, systematically update every relative path in every build configuration. The key transformation: references that went through `cubzh/` (the old submodule) now point directly to root-level dirs.

**Old path patterns → New path patterns** (from each build file's perspective):

| Build file location (old) | Build file location (new) | `cubzh/X` becomes | `common/` becomes | `deps/` (private) becomes |
|---|---|---|---|---|
| `private-repo-import/ios-macos/Particubes/` | `clients/ios-macos/Particubes/` | `../../../X` | `../../../common/` | `../../../deps/` |
| `private-repo-import/common/VXGameServer/` | `common/VXGameServer/` | `../../X` | stays `../` relative | `../../deps/` |
| `private-repo-import/android/app/src/main/cpp/` | `clients/android/app/src/main/cpp/` | resolve to root | resolve to root | resolve to root |
| `private-repo-import/ci/C/` | `ci/C/` | (Docker absolute paths) | (Docker absolute paths) | (Docker absolute paths) |

- [x] 2a: Fix Xcode project paths in `clients/ios-macos/Particubes/Blip.xcodeproj/project.pbxproj` — replaced 175 lines: `../../cubzh/` → `../../../`, `../../deps|common|ci` → `../../../`, fixed group-relative asset paths and absolute `/Users/` paths
- [x] 2b: Fix Xcode workspace and sub-project references — fixed 84 `cubzh/` refs in 8 bgfx/bx/bimg lib projects (iOS+macOS), 44+15 `../../deps/` refs in freetype+harfbuzz projects. Workspace data was already correct.
- [x] 2c: Fix `common/VXGameServer/CMakeLists.txt` and xptools.cmake — removed `cubzh/` from all REPO_ROOT_DIR-based paths, eliminated CUBZH_DIR variable
- [x] 2d: Fix `clients/android/` build paths — fixed `cubzh/` in CMakeLists.txt, xptools.cmake, and build.gradle; adjusted relative depth for all paths
- [x] 2e: Fix `ci/C/Makefile` — replaced `/cubzh/` with `/` for all Docker container absolute paths
- [x] 2f: Fix CI Dockerfiles — removed `cubzh/` from COPY source/dest paths in 3 Dockerfiles, 2 clang-format scripts; removed duplicate COPY lines
- [x] 2g: Fix `dockerfiles/*.Dockerfile` — fixed source paths in gameserver, hub-server, parent-dashboard Dockerfiles and dockerignore files
- [x] 2h: Fix remaining path references across the repo — fixed 77 `cubzh/` path refs in 12 files: WASM build, shell scripts, server Dockerfiles and dockerignores

## Phase 3: Fix iOS and macOS builds

With paths updated, get the Xcode workspace building for both iOS and macOS targets.

- [x] 3a: Fix Xcode file references in Blip.xcodeproj — already handled in Phase 2 (pbxproj paths). Fixed remaining scheme script paths: deptool `cubzh/deps/` → `../../../deps/`, removed broken `convert_env_to_xcconfig.sh` pre-actions from 5 schemes, fixed script internal path.
- [x] 3b: Fix freetype and harfbuzz sub-projects — already handled in Phase 2 (all `../../deps/` → `../../../deps/` changes)
- [x] 3c: Build and fix iOS target — fixed hasher Go code (removed cubzh/ prefix, removed .env dependency), replaced LUAU_MODULES_HASH_SALT preprocessor define with hardcoded salt in lua_require.cpp and VXGame.cpp, added env.xcconfig placeholder, created GoogleService-Info.plist placeholder
- [x] 3d: Build and fix macOS target — built successfully with no additional fixes needed
- [x] 3e: Add build scripts — created scripts/build-ios.sh and scripts/build-macos.sh as xcodebuild wrappers

## Phase 3.5: Push to GitHub

- [x] 3.5: Push `repo-restructure` branch to origin — pushed successfully (4 commits, small diff since origin/main already had Phases 1-2)

## Phase 3.6: Fix runtime issues in clients

Fix remaining path issues that prevent clients from running (not just building).

- [x] 3.6a: Fix xptools PROJECT_LUA_MODULES_PATH, PROJECT_BUNDLE_PATH, and CI paths — still referenced `cubzh/` in preprocessor defines, causing `openBundleFile` to return NULL at runtime

## Phase 4: Fix server builds

Get the game server and backend services building.

- [ ] 4a: Fix and test `common/VXGameServer/` CMake build (Linux cross-compilation from macOS or native Linux build)
- [ ] 4b: Fix and test Go service builds — verify `go build` works for key services in `go/cu.bzh/` (hub, gameserver management, fileserver, etc.)
- [ ] 4c: Fix and test Docker builds for backend services using updated Dockerfiles and docker-compose
- [ ] 4d: Create `scripts/build-gameserver.sh` — script to build the C++ game server
- [ ] 4e: Create `scripts/build-services.sh` — script to build/start backend services via docker-compose

## Phase 5: Documentation and build scripts

- [ ] 5a: Update `README.md` with new repository structure, getting started guide, and platform build overview
- [ ] 5b: Update `BUILD.md` with detailed build instructions for each platform (iOS, macOS, servers)
- [ ] 5c: Add a root `scripts/README.md` documenting all available build scripts and their usage
- [ ] 5d: Document server deployment process (Docker-based, docker-compose, environment setup)

## Phase 6: Fix Android build (deferred — needs Android Studio environment)

- [ ] 6a: Fix `clients/android/` Gradle and CMake paths
- [ ] 6b: Build and fix the Android app until it compiles
- [ ] 6c: Create `scripts/build-android.sh`

## Phase 7: Fix Windows build (deferred — needs Windows machine)

- [ ] 7a: Fix `clients/windows/` Visual Studio project paths
- [ ] 7b: Build and fix the Windows app until it compiles
- [ ] 7c: Create `scripts/build-windows.bat`

## Notes

- Phases 6 and 7 are deferred until the appropriate build environments are available
- The WASM/web target is kept in `clients/wasm/` for reference but is not a priority to fix
- The old `repo-restructure-old` branch has all the path fixes for reference
- Secret removal (Phase 2 of the open-sourcing plan) should be completed before pushing to remote
