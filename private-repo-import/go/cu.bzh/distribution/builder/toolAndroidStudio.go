package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
)

type AndroidStudioTool struct{}

func (t AndroidStudioTool) Name() string {
	return "Android Studio"
}

func (t AndroidStudioTool) IsInstalled() bool {
	// TODO: implement me
	return false
}

// GetTargetBundleVersion returns the bundle version of a target in an Android Studio project.
// The moduleDirectory is the directory containing the build.gradle file (usually "app").
func (t AndroidStudioTool) GetTargetVersionName(projectPath, moduleDirectory string) (string, error) {

	// Path to your app module's build.gradle file
	gradleFilePath := filepath.Join(projectPath, moduleDirectory, "build.gradle")

	// Read the file content
	gradleFileContent, err := os.ReadFile(gradleFilePath)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return "", err
	}

	// Define a regex to find versionName
	versionRegex, err := regexp.Compile(`versionName\s+"(.+?)"`)
	if err != nil {
		return "", err
	}

	// Search for the version number
	matches := versionRegex.FindStringSubmatch(string(gradleFileContent))
	if len(matches) <= 1 {
		return "", errors.New("versionName not found in gradle file")
	}

	result := matches[1] // matches[1] is the captured version number
	return result, nil
}
