package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
)

func targetAppleCommonBuild(cubzhTarget, xcodeTarget string, opts XcodeToolBuildOpts) error {

	if cubzhXcodeProjectPath == "" {
		return errors.New("cubzhXcodeProjectPath is not set")
	}
	xcodeProjectPath := cubzhXcodeProjectPath

	xcodeProjectVersion, err := toolXcode.GetTargetBundleVersion(xcodeProjectPath, xcodeTarget)
	if err != nil {
		return err
	}

	// Build Xcode target
	err = toolXcode.BuildScheme(xcodeProjectPath, xcodeTarget, opts)
	if err != nil {
		return err
	}

	// Locate built artifacts and archive directories
	artifacts := []string{}
	{
		modeName := "Debug"
		if opts.ReleaseMode {
			modeName = "Release"
		}
		basePath := filepath.Join(opts.DerivedDataPath, "Build", "Products", modeName)
		cubzhAppPath := filepath.Join(basePath, "Cubzh.app")
		cubzhDsymPath := filepath.Join(basePath, "Cubzh.app.dSYM")
		cubzhAppPathTarGz := cubzhAppPath + ".tar.gz"
		cubzhDsymPathTarGz := cubzhDsymPath + ".tar.gz"

		// Archive the .app bundle
		err = TarGzDirectory(cubzhAppPath, cubzhAppPathTarGz)
		if err != nil {
			return err
		}
		artifacts = append(artifacts, cubzhAppPathTarGz)

		// Archive the .dSYM bundle
		if opts.ReleaseMode {
			err = TarGzDirectory(cubzhDsymPath, cubzhDsymPathTarGz)
			if err != nil {
				return err
			}
			artifacts = append(artifacts, cubzhDsymPathTarGz)
		}
	}

	// Upload artifacts to DigitalOcean Spaces
	{
		doStorage, err := NewDigitalOceanObjectStorage("DO00UKEPTJKJNBL788WE", os.Getenv("CUBZH_DIGITALOCEAN_SPACES_API_SECRET"), "nyc3", "cubzh-builds")
		if err != nil {
			return err
		}
		for _, artifactPath := range artifacts {

			// extract filename from path
			filename := filepath.Base(artifactPath)
			fmt.Println("📦 Uploading artifact:", artifactPath)

			// get io.ReadSeeker for the file
			file, err := os.Open(artifactPath)
			if err != nil {
				return err
			}
			err = doStorage.UploadFile("darwin/"+xcodeProjectVersion+"/"+cubzhTarget+"/"+filename, file)
			if err != nil {
				return err
			}
			fmt.Println("✅")
		}
	}

	return nil
}

// Steam target for macOS

type DarwinSteamTarget struct{}

func (t DarwinSteamTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t DarwinSteamTarget) Name() string {
	return targetDarwinSteam
}

func (t DarwinSteamTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin}
}

func (t DarwinSteamTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t DarwinSteamTarget) CompatibleToolset() []Tool {
	return []Tool{toolXcode}
}

func (t DarwinSteamTarget) XcodeTarget() string {
	return "macos"
}

// Checks if the target can be built on the current machine
// - tools must be installed and up-to-date
// - version of Cubzh must have been increased
func (t DarwinSteamTarget) RunPrebuildChecks() error {
	const storeName = "Steam"

	if cubzhXcodeProjectPath == "" {
		return errors.New("cubzhXcodeProjectPath is not set")
	}

	// get latest version of Cubzh published on store
	latestVersionPublished, err := getCubzhVersionFromSteam()
	if err != nil {
		return err
	}

	// get current version of Cubzh in local Xcode project
	xcodeProjectVersion, err := toolXcode.GetTargetBundleVersion(cubzhXcodeProjectPath, t.XcodeTarget())
	if err != nil {
		return err
	}

	// Compare the two versions
	if !ensureVersionIsGreaterThan(xcodeProjectVersion, latestVersionPublished) {
		return fmt.Errorf("Cubzh version in Xcode project (%s) is not greater than the latest one on %s (%s)", xcodeProjectVersion, storeName, latestVersionPublished)
	}

	return nil
}

func (t DarwinSteamTarget) Build(opts BuildOpts) error {

	const derivedDataPath = "./xcode_build_output"
	const installSourceValue = CubzhBuildMacroInstallSourceSteam

	err := targetAppleCommonBuild(t.Name(), t.XcodeTarget(), XcodeToolBuildOpts{
		BuildOpts: opts,
		Macros: BuildMacros{
			CubzhBuildMacroInstallSource: installSourceValue,
		},
		DerivedDataPath: derivedDataPath,
	})

	return err
}

// Epic Games Store target for macOS

type DarwinEGSTarget struct{}

func (t DarwinEGSTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t DarwinEGSTarget) Name() string {
	return targetDarwinEpicGamesStore
}

func (t DarwinEGSTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin}
}

func (t DarwinEGSTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t DarwinEGSTarget) CompatibleToolset() []Tool {
	return []Tool{toolXcode}
}

func (t DarwinEGSTarget) XcodeTarget() string {
	return "macos"
}

func (t DarwinEGSTarget) RunPrebuildChecks() error {

	// Note (gaetan)
	// -----------------
	// Epic Games Store web page doesn't contain the app version
	// so we can't compare it with the local Xcode project version
	// https://store.epicgames.com/fr/p/cubzh-3cc767

	return nil
}

func (t DarwinEGSTarget) Build(opts BuildOpts) error {

	const derivedDataPath = "./xcode_build_output"
	const installSourceValue = CubzhBuildMacroInstallSourceEpicGamesStore

	err := targetAppleCommonBuild(t.Name(), t.XcodeTarget(), XcodeToolBuildOpts{
		BuildOpts: opts,
		Macros: BuildMacros{
			CubzhBuildMacroInstallSource: installSourceValue,
		},
		DerivedDataPath: derivedDataPath,
	})

	return err
}

// Mac App Store target for macOS

type DarwinMacAppStoreTarget struct{}

func (t DarwinMacAppStoreTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t DarwinMacAppStoreTarget) Name() string {
	return targetDarwinMacAppStore
}

func (t DarwinMacAppStoreTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin}
}

func (t DarwinMacAppStoreTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t DarwinMacAppStoreTarget) CompatibleToolset() []Tool {
	return []Tool{toolXcode}
}

func (t DarwinMacAppStoreTarget) XcodeTarget() string {
	return "macos"
}

// We need to publish Cubzh to the Mac App Store before uncommenting those checks
func (t DarwinMacAppStoreTarget) RunPrebuildChecks() error {
	// const storeName = "Mac App Store"

	// if cubzhXcodeProjectPath == "" {
	// 	return errors.New("cubzhXcodeProjectPath is not set")
	// }

	// // get latest version of Cubzh published on store
	// latestVersionPublished, err := getCubzhVersionFromMacAppStore()
	// if err != nil {
	// 	return err
	// }

	// // get current version of Cubzh in local Xcode project
	// xcodeProjectVersion, err := toolXcode.GetTargetBundleVersion(cubzhXcodeProjectPath, t.XcodeTarget())
	// if err != nil {
	// 	return err
	// }

	// // Compare the two versions
	// if !ensureVersionIsGreaterThan(xcodeProjectVersion, latestVersionPublished) {
	// 	return fmt.Errorf("Cubzh version in Xcode project (%s) is not greater than the latest one on %s (%s)", xcodeProjectVersion, storeName, latestVersionPublished)
	// }

	return nil
}

func (t DarwinMacAppStoreTarget) Build(opts BuildOpts) error {

	const derivedDataPath = "./xcode_build_output"
	const installSourceValue = CubzhBuildMacroInstallSourceMacAppStore

	err := targetAppleCommonBuild(t.Name(), t.XcodeTarget(), XcodeToolBuildOpts{
		BuildOpts: opts,
		Macros: BuildMacros{
			CubzhBuildMacroInstallSource: installSourceValue,
		},
		DerivedDataPath: derivedDataPath,
	})

	return err
}

// App Store target for iOS/iPadOS

type IOSAppStoreTarget struct{}

func (t IOSAppStoreTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t IOSAppStoreTarget) Name() string {
	return targetIOSAppStore
}

func (t IOSAppStoreTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin}
}

func (t IOSAppStoreTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t IOSAppStoreTarget) CompatibleToolset() []Tool {
	return []Tool{toolXcode}
}

func (t IOSAppStoreTarget) XcodeTarget() string {
	return "ios"
}

func (t IOSAppStoreTarget) RunPrebuildChecks() error {
	const storeName = "iOS App Store"

	if cubzhXcodeProjectPath == "" {
		return errors.New("cubzhXcodeProjectPath is not set")
	}

	// get latest version of Cubzh published on store
	latestVersionPublished, err := getCubzhVersionFromIOSAppStore()
	if err != nil {
		return err
	}

	// get current version of Cubzh in local Xcode project
	xcodeProjectVersion, err := toolXcode.GetTargetBundleVersion(cubzhXcodeProjectPath, t.XcodeTarget())
	if err != nil {
		return err
	}

	// Compare the two versions
	if !ensureVersionIsGreaterThan(xcodeProjectVersion, latestVersionPublished) {
		return fmt.Errorf("Cubzh version in Xcode project (%s) is not greater than the latest one on %s (%s)", xcodeProjectVersion, storeName, latestVersionPublished)
	}

	return nil
}

func (t IOSAppStoreTarget) Build(opts BuildOpts) error {

	const derivedDataPath = "./xcode_build_output"
	const installSourceValue = CubzhBuildMacroInstallSourceIOSAppStore

	err := targetAppleCommonBuild(t.Name(), t.XcodeTarget(), XcodeToolBuildOpts{
		BuildOpts: opts,
		Macros: BuildMacros{
			CubzhBuildMacroInstallSource: installSourceValue,
		},
		DerivedDataPath: derivedDataPath,
	})

	return err
}

// ------------------------------
// Utility functions
// ------------------------------

func getCubzhVersionFromSteam() (string, error) {
	return getCubzhVersionFromWebPage(steamCubzhNewsURL, `Update (\d+\.\d+\.\d+) \\u2728`)
}

func getCubzhVersionFromIOSAppStore() (string, error) {
	return getCubzhVersionFromWebPage(iOSAppStoreCubzhURL, `>Version (\d+\.\d+\.\d+)</p>`)
}

func getCubzhVersionFromMacAppStore() (string, error) {
	return getCubzhVersionFromWebPage(MacAppStoreCubzhURL, `>Version (\d+\.\d+\.\d+)</p>`)
}
