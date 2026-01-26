package main

import (
	"errors"
	"fmt"
	"os/exec"
	"regexp"
)

type XcodeTool struct{}

type XcodeToolBuildOpts struct {
	BuildOpts
	DerivedDataPath string
	Macros          BuildMacros
}

func (t XcodeTool) Name() string {
	return "Xcode"
}

func (t XcodeTool) IsInstalled() bool {
	// Run `xcodebuild -version` to check if Xcode is installed
	cmd := exec.Command("xcodebuild", "-version")

	// Run the command and capture any error
	_, err := cmd.Output()
	if err != nil {
		// Xcode is not installed
		return false
	}

	// Xcode is installed
	return true
}

func (t XcodeTool) BuildScheme(xcworkspacePath, scheme string, opts XcodeToolBuildOpts) error {
	var err error

	buildMode := "Debug"
	if opts.ReleaseMode {
		buildMode = "Release"
	}

	// Construct the xcodebuild command
	cmdElems := []string{
		"xcodebuild",
		"-workspace", xcworkspacePath,
		"-scheme", scheme,
		"-configuration", buildMode}

	// Add the derived data path to the command
	if opts.DerivedDataPath != "" {
		cmdElems = append(cmdElems, "-derivedDataPath", opts.DerivedDataPath)
	}

	// Add the macros to the command
	if len(opts.Macros) > 0 {
		preprocStr := "$(inherited)"
		for k, v := range opts.Macros {
			preprocStr += " " + k + `="` + v + `"`
		}
		// add the macros to the command
		cmdElems = append(cmdElems, `GCC_PREPROCESSOR_DEFINITIONS=`+preprocStr)
	}

	// Define the xcodebuild command to get build settings
	cmd := exec.Command(cmdElems[0], cmdElems[1:]...)

	// Run the command and capture the output
	output, err := cmd.CombinedOutput()
	if err != nil {
		fmt.Println(string(output))
		return errors.New("error running xcodebuild: " + err.Error())
	}

	return nil
}

// GetTargetBundleVersion returns the bundle version of a target in an Xcode workspace
func (t XcodeTool) GetTargetBundleVersion(workspacePath, schemeName string) (string, error) {

	if workspacePath == "" {
		return "", errors.New("workspace path is empty")
	}

	if schemeName == "" {
		return "", errors.New("scheme name is empty")
	}

	// TODO: make sure the OS is darwin
	// TODO: make sure xcodebuild is installed

	// Define the xcodebuild command to get build settings
	cmd := exec.Command("xcodebuild", "-workspace", workspacePath, "-scheme", schemeName, "-showBuildSettings")

	// Run the command and capture the output
	output, err := cmd.CombinedOutput()
	if err != nil {
		return "", errors.New("error running xcodebuild: " + err.Error())
	}

	// Convert the output to a string
	outputStr := string(output)

	// Define a regular expression to capture the MARKETING_VERSION value
	versionPattern := regexp.MustCompile(`MARKETING_VERSION = (\d+\.\d+\.\d+)`)

	// Search for the version number
	matches := versionPattern.FindStringSubmatch(outputStr)
	if len(matches) <= 1 {
		return "", errors.New("MARKETING_VERSION not found in xcodebuild output")
	}

	result := matches[1] // matches[1] is the captured version number
	return result, nil
}
