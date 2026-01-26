package main

import (
	"bytes"
	"fmt"
	"os"
	"slices"

	"github.com/spf13/cobra"
)

const (
	// Environment variables' names
	envVarCubzhXcodeWorkspacePath      = "CUBZH_XCODE_WORKSPACE_PATH"
	envVarCubzhVisualStudioProjectPath = "CUBZH_VISUALSTUDIO_PROJECT_PATH"

	// target names
	targetAndroidGooglePlay     = "android_googleplay"
	targetDarwinSteam           = "darwin_steam"
	targetDarwinEpicGamesStore  = "darwin_epicgamesstore"
	targetDarwinMacAppStore     = "darwin_macappstore"
	targetIOSAppStore           = "ios_appstore"
	targetWasm                  = "wasm"
	targetWasmDiscord           = "wasm_discord"
	targetWindowsSteam          = "windows_steam"
	targetWindowsEpicGamesStore = "windows_epicgamesstore"
	targetWindowsMicrosoftStore = "windows_microsoftstore"

	// URLs
	steamCubzhNewsURL       = "https://store.steampowered.com/news/app/1386770"
	iOSAppStoreCubzhURL     = "https://apps.apple.com/fr/app/cubzh/id1478257849"
	MacAppStoreCubzhURL     = "" // TODO: update this URL
	googlePlayStoreCubzhURL = "https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android"

	// Build
	CubzhBuildMacroInstallSource               = "INSTALL_SOURCE"
	CubzhBuildMacroInstallSourceSteam          = "Steam"
	CubzhBuildMacroInstallSourceEpicGamesStore = "Epic Games Store"
	CubzhBuildMacroInstallSourceIOSAppStore    = "iOS App Store"
	CubzhBuildMacroInstallSourceGooglePlay     = "Google Play"
	CubzhBuildMacroInstallSourceMacAppStore    = "Mac App Store"
	CubzhBuildMacroInstallSourceMicrosoftStore = "Microsoft Store"
)

var (
	// TODO: get from env vars
	cubzhXcodeProjectPath         = "/Users/gaetan/projects/voxowl/particubes-private/ios-macos/particubes.xcworkspace"
	cubzhAndroidStudioProjectPath = "/Users/gaetan/projects/voxowl/particubes-private/android"

	// tools
	toolXcode         = XcodeTool{}
	toolAndroidStudio = AndroidStudioTool{}
	toolDocker        = DockerTool{}
	toolVisualStudio  = VisualStudioTool{}

	allTargets = []Target{

		// Android targets
		AndroidGooglePlayTarget{},
		// AndroidAmazonAppStoreTarget{},
		// AndroidSamsungGalaxyStoreTarget{},
		// AndroidHuaweiAppGalleryTarget{},

		// MacOS targets
		DarwinSteamTarget{},
		DarwinEGSTarget{},
		DarwinMacAppStoreTarget{},

		// iOS/iPadOS targets
		IOSAppStoreTarget{},

		// Wasm targets
		WasmTarget{},
		WasmDiscordTarget{},

		// Windows targets
		WindowsSteamTarget{},
		WindowsEGSTarget{},
		// WindowsMicrosoftStoreTarget{},
	}

	allTargetsByName = make(map[string]Target)

	// Build configuration
	release = true
)

func main() {
	// initialize the map of targets by name
	for _, target := range allTargets {
		allTargetsByName[target.Name()] = target
	}

	var rootCmd = genRootCmd()
	rootCmd.AddCommand(genBuildCmd())
	rootCmd.AddCommand(genDebugCmd())
	rootCmd.Execute()
}

func genRootCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "cubzh-build",
		Short: "Cubzh builder is a tool to compile Cubzh for multiple platforms",
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Printf("✨ Cubzh builder (%s/%s)\n", platformOS(), platformArch())

			fmt.Println("🎯 Targets availability:")
			targetsCompatibility := computeTargetsCompatibilityStatus()
			for _, tc := range targetsCompatibility {
				fmt.Println("  " + tc.ToString())
			}
		},
	}
}

// TODO: add Debug/Release flag
// TODO: add Upload (Y/N) flag
func genBuildCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "build",
		Short: "Build Cubzh for one or more targets",
		Run:   buildCommand,
	}
}

// TODO: add Clean command
// func genCleanCmd() *cobra.Command {
// 	return &cobra.Command{
// 		Use:   "clean",
// 		Short: "Clean Cubzh for one or more targets",
// 		Run:   cleanCommand,
// 	}
// }

func genDebugCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "debug",
		Short: "Debug code",
		Run:   debugCommand,
	}
}

// examples:
// cubzh-build build
// cubzh-build build darwin_steam
// cubzh-build build darwin_steam android_googleplay
func buildCommand(cmd *cobra.Command, args []string) {
	// construct the list of targets to build
	targetsToBuild := make([]Target, 0)
	if len(args) == 0 {
		// build all targets that can be built on this machine
		targets := computeTargetsCompatibilityStatus()
		for _, t := range targets {
			if t.IsSupported {
				targetsToBuild = append(targetsToBuild, allTargetsByName[t.TargetName])
			}
		}
	} else {
		// build only the specified targets
		for _, targetName := range args {
			if t, ok := allTargetsByName[targetName]; ok {
				targetsToBuild = append(targetsToBuild, t)
			}
		}
	}

	fmt.Println("🚧 Building Cubzh...")

	// build the targets
	for _, target := range targetsToBuild {
		fmt.Println("  " + target.ToString())
		fmt.Println("    ⚙️ Prebuild checks [" + target.Name() + "]...")
		err := target.RunPrebuildChecks()
		if err != nil {
			fmt.Printf("❌ %v\n", err)
			continue
		} else {
			fmt.Println("    ✅ Prebuild checks passed")
		}

		// Define the build options
		buildOpts := BuildOpts{
			ReleaseMode:  release,
			ForceRebuild: release == true, // force clean/rebuild for release builds
		}

		fmt.Println("    ⚙️ Building [" + target.Name() + "]...")
		fmt.Printf("       Options: [ReleaseMode=%+v] [ForceRebuild=%+v]\n", buildOpts.ReleaseMode, buildOpts.ForceRebuild)

		err = target.Build(buildOpts)
		if err != nil {
			fmt.Printf("❌ %v\n", err)
		} else {
			fmt.Println("    ✅ Build succeeded")
		}
	}
}

func debugCommand(cmd *cobra.Command, args []string) {
	fmt.Println("Debugging...")
	doStorage, err := NewDigitalOceanObjectStorage("DO00UKEPTJKJNBL788WE", os.Getenv("CUBZH_DIGITALOCEAN_SPACES_API_SECRET"), "nyc3", "cubzh-builds")
	if err != nil {
		fmt.Println("❌", err)
		return
	}
	err = doStorage.UploadFile("test.txt", bytes.NewReader([]byte("Hello, World!")))
	if err != nil {
		fmt.Println("❌", err)
		return
	}
	fmt.Println("✅")
}

func computeTargetsCompatibilityStatus() map[string]targetCompatbilityStatus {
	result := make(map[string]targetCompatbilityStatus)

	for _, target := range allTargets {
		result[target.Name()] = targetCompatbilityStatus{
			TargetName:  target.Name(),
			IsSupported: canTargetBeBuilt(target),
			Error:       nil,
		}
	}

	return result
}

func canTargetBeBuilt(t Target) bool {

	if !slices.Contains(t.CompatibleOSes(), platformOS()) {
		// machine's OS is not compatible with the target
		return false
	}

	if !slices.Contains(t.CompatibleArchs(), platformArch()) {
		// machine's Arch is not compatible with the target
		return false
	}

	for _, tool := range t.CompatibleToolset() {
		if !tool.IsInstalled() {
			// tool is missing
			return false
		}
	}

	return true
}
