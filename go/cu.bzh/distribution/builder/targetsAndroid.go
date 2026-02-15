package main

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"os/exec"
)

// Android target

type AndroidGooglePlayTarget struct{}

func (t AndroidGooglePlayTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t AndroidGooglePlayTarget) Name() string {
	return targetAndroidGooglePlay
}

func (t AndroidGooglePlayTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin, platformOSLinux, platformOSWindows}
}

func (t AndroidGooglePlayTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t AndroidGooglePlayTarget) CompatibleToolset() []Tool {
	return []Tool{toolAndroidStudio}
}

func (t AndroidGooglePlayTarget) RunPrebuildChecks() error {
	const storeName = "Google Play Store"

	if cubzhAndroidStudioProjectPath == "" {
		return errors.New("cubzhAndroidStudioProjectPath is not set")
	}

	// get latest version of Cubzh published on store
	latestVersionPublished, err := getCubzhVersionFromGooglePlayStore()
	if err != nil {
		return err
	}

	// get current version of Cubzh in local Xcode project
	androidProjectVersion, err := toolAndroidStudio.GetTargetVersionName(cubzhAndroidStudioProjectPath, "app")
	if err != nil {
		return err
	}

	// Compare the two versions
	if !ensureVersionIsGreaterThan(androidProjectVersion, latestVersionPublished) {
		return fmt.Errorf("Cubzh version in Android Studio project (%s) is not greater than the latest one on %s (%s)", androidProjectVersion, storeName, latestVersionPublished)
	}

	return nil
}

func (t AndroidGooglePlayTarget) Clean(opts BuildOpts) error {
	// JAVA_HOME=/Applications/Android\ Studio.app/Contents/jbr/Contents/Home

	if cubzhAndroidStudioProjectPath == "" {
		return errors.New("cubzhAndroidStudioProjectPath is not set")
	}

	// Ensure that the JAVA_HOME environment variable is set
	// This is required by the Gradle wrapper
	if os.Getenv("JAVA_HOME") == "" {
		return errors.New("JAVA_HOME environment variable is not set")
	}

	// Path to the Android project directory
	projectDir := cubzhAndroidStudioProjectPath

	output, err := runCommand(projectDir, "./gradlew", "--no-configuration-cache", "clean")
	if err != nil {
		fmt.Printf("Clean failed: Gradle error: %v\n", err)
		fmt.Printf("Gradle stderr: %s\n", output)
		return err
	}

	// Output the result
	fmt.Println("Clean completed successfully!")
	fmt.Println(output)

	return nil // success
}

func (t AndroidGooglePlayTarget) Build(opts BuildOpts) error {

	// JAVA_HOME=/Applications/Android\ Studio.app/Contents/jbr/Contents/Home

	if cubzhAndroidStudioProjectPath == "" {
		return errors.New("cubzhAndroidStudioProjectPath is not set")
	}

	// Ensure that the JAVA_HOME environment variable is set
	// This is required by the Gradle wrapper
	if os.Getenv("JAVA_HOME") == "" {
		return errors.New("JAVA_HOME environment variable is not set")
	}

	if opts.ReleaseMode == false {
		return errors.New("building non-release builds is not implemented")
	}

	// Path to the Android project directory
	projectDir := cubzhAndroidStudioProjectPath

	gradleArgs := []string{"--no-configuration-cache"}
	if opts.ForceRebuild {
		// add the "clean" gradle command
		gradleArgs = append(gradleArgs, "clean")
	}
	gradleArgs = append(gradleArgs, "bundleRelease")

	output, err := runCommand(projectDir, "./gradlew", gradleArgs...)
	if err != nil {
		fmt.Printf("Build failed: Gradle error: %v\n", err)
		fmt.Printf("Gradle stderr: %s\n", output)
		return err
	}

	// Output the result
	fmt.Println("Build completed successfully!")
	fmt.Println(output)

	// Signed AAB file will be located in the following directory:
	// <projectDir>/app/build/outputs/bundle/release/
	fmt.Printf("The signed bundle should be located at: %s/app/build/outputs/bundle/release/\n", projectDir)

	return nil // success
}

// ------------------------------
// Utility functions
// ------------------------------

func getCubzhVersionFromGooglePlayStore() (string, error) {
	// We use the first line of the "What's New" section to get the version number of the latest update
	// It looks like: `### 0.1.10 (November 16, 2024)`
	return getCubzhVersionFromWebPage(googlePlayStoreCubzhURL, `### (\d+\.\d+\.\d+) `)
}

// Runs the command and returns the std output.
// Returns an error if no command is provided.
// On success, output is stdout.
// On error, output is stderr
func runCommand(dir string, cmd string, args ...string) (output string, err error) {
	if cmd == "" {
		err = errors.New("no command provided")
		return
	}

	// Create the command
	cmdObj := exec.Command(cmd, args...)
	cmdObj.Dir = dir

	// Capture the output
	var stdout bytes.Buffer
	var stderr bytes.Buffer
	cmdObj.Stdout = &stdout
	cmdObj.Stderr = &stderr

	// Run the command
	err = cmdObj.Run()

	if err != nil {
		output = stderr.String()
	} else {
		output = stdout.String()
	}

	return
}
