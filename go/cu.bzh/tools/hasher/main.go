package main

import (
	"crypto/md5"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"

	"github.com/joho/godotenv"
)

const (
	hashBlockStart = "// HASHES START\n"
	hashBlockEnd   = "// HASHES END"
)

var (
	modulesDir = "/cubzh/lua/modules"
	luaRequire = "/common/VXLuaSandbox/lua_require.cpp"
)

func main() {
	var err error
	var gitRootPath string

	// Read first argument as the git root path
	if len(os.Args) > 1 {
		gitRootPath = os.Args[1]
	} else {
		// find root of Git repository
		gitRootPath, err = findGitRoot()
		if err != nil {
			fmt.Printf("Error finding git root: %s\n", err)
			return
		}
	}

	modulesDir = filepath.Join(gitRootPath, modulesDir)
	luaRequire = filepath.Join(gitRootPath, luaRequire)

	// Load .env file from git root
	envPath := filepath.Join(gitRootPath, ".env")
	if err := godotenv.Load(envPath); err != nil {
		log.Fatalf("Warning: Could not load .env file from %s: %v", envPath, err)
	}

	// Get salt from environment variable
	saltStr := os.Getenv("LUAU_MODULES_HASH_SALT")
	if saltStr == "" {
		log.Fatalf("Warning: LUAU_MODULES_HASH_SALT not set, using default value")
	}
	salt := []byte(saltStr)
	block := "static std::map<std::string, std::string> hashes = {\n"

	filepath.Walk(modulesDir, func(path string, info os.FileInfo, err error) error {

		if err != nil {
			fmt.Printf("Error accessing path: %s\n", err)
			return nil
		}

		if !info.IsDir() && strings.HasSuffix(info.Name(), ".lua") {
			content, err := os.ReadFile(path)
			if err != nil {
				fmt.Printf("Error reading file %s: %s\n", path, err)
				return nil
			}
			hash := md5.Sum(append(salt, content...))
			block += fmt.Sprintf("    {\"%s\", \"%x\"},\n", filepath.Base(path), hash)
		}

		return nil
	})

	block += "};\n"

	// Use os.ReadFile and os.WriteFile to perform file operations
	fileBytes, err := os.ReadFile(luaRequire)
	if err != nil {
		fmt.Printf("Error reading file %s: %s\n", luaRequire, err)
		return
	}

	newContent := replaceSubstring(string(fileBytes), hashBlockStart, hashBlockEnd, block)

	err = os.WriteFile(luaRequire, []byte(newContent), 0644)
	if err != nil {
		fmt.Printf("Error writing temp file: %s\n", err)
		return
	}

	fmt.Println("✅ Lua modules hashes updated.")
}

func findGitRoot() (string, error) {
	dir, err := os.Getwd()
	if err != nil {
		return "", err
	}

	for {
		gitDir := filepath.Join(dir, ".git")
		if _, err := os.Stat(gitDir); err == nil {
			return dir, nil
		}

		parentDir := filepath.Dir(dir)
		if parentDir == dir {
			break // reached the filesystem root
		}
		dir = parentDir
	}

	return "", fmt.Errorf("git root not found")
}

func replaceSubstring(input, startMarker, endMarker, replacement string) string {
	startIndex := strings.Index(input, startMarker)
	if startIndex == -1 {
		// Start marker not found
		return input
	}

	endIndex := strings.Index(input[startIndex:], endMarker)
	if endIndex == -1 {
		// End marker not found
		return input
	}
	endIndex += startIndex //+ len(endMarker)

	return input[:startIndex+len(startMarker)] + replacement + input[endIndex:]
}
