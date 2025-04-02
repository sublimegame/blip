package deptool

import (
	"encoding/json"
	"fmt"
	"os"
	"slices"
)

type Dependency struct {
	Version   string   `json:"version"`
	UseSource []string `json:"use_source"`
}

type Config struct {
	Deps map[string]Dependency `json:"Deps"`
	// ignore the other fields
}

func parseConfig(configFilePath string) (Config, error) {

	configData, err := os.ReadFile(configFilePath)
	if err != nil {
		return Config{}, fmt.Errorf("failed to read config file: %w", err)
	}

	var config Config
	err = json.Unmarshal(configData, &config)
	if err != nil {
		return Config{}, fmt.Errorf("failed to parse config file: %w", err)
	}

	return config, nil
}

// Autoconfigure configures the dependencies for the given target platforms
// It downloads the dependencies listed in bundle/config.json and activates them
func Autoconfigure(objectStorageBuildFunc ObjectStorageBuildFunc, depsDirPath, configFilePath string, platforms []string) error {

	if configFilePath == "" {
		return fmt.Errorf("config file path is required")
	}

	// parse config file
	config, err := parseConfig(configFilePath)
	if err != nil {
		return fmt.Errorf("failed to parse config file: %w", err)
	}

	// get object storage credentials

	// for each dependency, download the dependency
	for depName, depInfo := range config.Deps {

		force := false
		for _, platform := range platforms {

			// if the platform is in use_source, use the "source" platform
			if slices.Contains(depInfo.UseSource, platform) {
				platform = PlatformSource
			}

			_, exists := areDependencyFilesInstalled(depsDirPath, depName, depInfo.Version, platform)
			if !exists || force {
				err = DownloadArtifacts(objectStorageBuildFunc, depsDirPath, depName, depInfo.Version, platform, force)
				if err != nil {
					return fmt.Errorf("failed to download dependency (%s|%s): %w", depName, depInfo.Version, err)
				}
			} else {
				fmt.Printf("✅ dependency [%s][%s] already installed\n", depName, depInfo.Version)
			}
		}
	}

	// for each dependency, activate the dependency version
	for depName, depInfo := range config.Deps {
		err = ActivateDependency(depsDirPath, depName, depInfo.Version)
		if err != nil {
			return fmt.Errorf("failed to activate dependency [%s][%s]: %w", depName, depInfo.Version, err)
		}
	}

	return nil
}
