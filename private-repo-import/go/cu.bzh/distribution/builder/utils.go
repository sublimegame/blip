package main

import (
	"archive/tar"
	"archive/zip"
	"compress/gzip"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

// ensureVersionIsGreaterThan returns true if versionHigher is greater than versionLower, false otherwise.
func ensureVersionIsGreaterThan(versionHigher, versionLower string) bool {
	return compareVersions(versionHigher, versionLower) > 0
}

// compareVersions compares two version strings.
// Returns 1 if version1 > version2, -1 if version1 < version2, and 0 if they are equal.
func compareVersions(version1, version2 string) int {
	// Split versions into parts
	v1Parts := strings.Split(version1, ".")
	v2Parts := strings.Split(version2, ".")

	// Determine the maximum length to avoid index out of range issues
	maxLen := len(v1Parts)
	if len(v2Parts) > maxLen {
		maxLen = len(v2Parts)
	}

	// Compare each part as an integer
	for i := 0; i < maxLen; i++ {
		var v1, v2 int

		// Parse v1 part if within range
		if i < len(v1Parts) {
			v1, _ = strconv.Atoi(v1Parts[i])
		}

		// Parse v2 part if within range
		if i < len(v2Parts) {
			v2, _ = strconv.Atoi(v2Parts[i])
		}

		// Compare the individual parts
		if v1 > v2 {
			return 1
		} else if v1 < v2 {
			return -1
		}
	}

	// Versions are equal
	return 0
}

// ZipDirectory compresses a directory (sourceDir) into a zip file (targetFile).
func ZipDirectory(sourceDir, targetFile string) error {
	// Create the target zip file.
	zipFile, err := os.Create(targetFile)
	if err != nil {
		return err
	}
	defer zipFile.Close()

	// Initialize the zip writer.
	zipWriter := zip.NewWriter(zipFile)
	defer zipWriter.Close()

	// Walk through every file in the source directory.
	return filepath.Walk(sourceDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		// Skip the directory itself in the zip file structure.
		if path == sourceDir {
			return nil
		}

		// Create the zip file entry name by trimming the source directory prefix.
		relPath, err := filepath.Rel(sourceDir, path)
		if err != nil {
			return err
		}

		// If it's a directory, create a folder entry in the zip file.
		if info.IsDir() {
			_, err := zipWriter.Create(relPath + "/")
			return err
		}

		// For files, create a file entry in the zip file.
		fileInZip, err := zipWriter.Create(relPath)
		if err != nil {
			return err
		}

		// Open the file to be added to the zip.
		file, err := os.Open(path)
		if err != nil {
			return err
		}
		defer file.Close()

		// Copy the file contents into the zip entry.
		_, err = io.Copy(fileInZip, file)
		return err
	})
}

// TarGzDirectory compresses the specified directory into a .tar.gz file
func TarGzDirectory(sourceDir, targetFile string) error {
	// Create the .tar.gz file
	file, err := os.Create(targetFile)
	if err != nil {
		return fmt.Errorf("could not create file %s: %v", targetFile, err)
	}
	defer file.Close()

	// Create a new gzip writer
	gzipWriter := gzip.NewWriter(file)
	defer gzipWriter.Close()

	// Create a new tar writer
	tarWriter := tar.NewWriter(gzipWriter)
	defer tarWriter.Close()

	// Walk through the directory
	return filepath.Walk(sourceDir, func(filePath string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		// Skip the root directory
		if filePath == sourceDir {
			return nil
		}

		// Get the relative path to add to the tar header
		relPath, err := filepath.Rel(sourceDir, filePath)
		if err != nil {
			return err
		}

		// Create the tar header
		header, err := tar.FileInfoHeader(info, relPath)
		if err != nil {
			return err
		}
		header.Name = relPath

		// Write the header to the tar file
		if err := tarWriter.WriteHeader(header); err != nil {
			return err
		}

		// If it's a directory, there's no content to write, so return
		if info.IsDir() {
			return nil
		}

		// Open the file to copy its content into the tar archive
		file, err := os.Open(filePath)
		if err != nil {
			return err
		}
		defer file.Close()

		// Copy the file content into the tar writer
		if _, err := io.Copy(tarWriter, file); err != nil {
			return err
		}

		return nil
	})
}

func fetchHTTPContent(url string) (string, error) {

	// Fetch the page content
	resp, err := http.Get(url)
	if err != nil {
		return "", errors.New("error fetching the URL: " + err.Error())
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", errors.New("received non-200 response code: " + fmt.Sprint(resp.StatusCode))
	}

	// Read the response body
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", errors.New("error reading the response body: " + err.Error())
	}

	// Convert body to string and return
	return string(body), nil
}

func getCubzhVersionFromWebPage(url, pattern string) (string, error) {

	// fetch web content
	content, err := fetchHTTPContent(url)
	if err != nil {
		return "", errors.New("error fetching web content: " + err.Error())
	}

	// <pattern> must contain `(\d+\.\d+\.\d+)` to capture the version number
	updatePattern, err := regexp.Compile(pattern)
	if err != nil {
		return "", errors.New("error compiling the regexp pattern: " + err.Error())
	}

	// Search for the first occurrence of the pattern and capture the version number
	matches := updatePattern.FindStringSubmatch(content)
	if len(matches) <= 1 {
		return "", errors.New("no update version found matching the pattern")
	}

	// matches[0] is the full match, matches[1] is the captured version number between the parentheses
	versionResult := matches[1]

	return versionResult, nil
}
