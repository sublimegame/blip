package diff

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
)

// ----------------------------------------------
// DIFF PARSING TESTS
// ----------------------------------------------

// func TestParseDiff1(t *testing.T) {
// 	// This diff is valid except for the " end" suffix in the hunk header
// 	diffText := `@@ -21,6 +21,6 @@ end
//  -- (space bar on PC, button 1 on mobile)
//  Client.Action1 = function()
//      if Player.IsOnGround then
// -        Player.Velocity.Y = 100
// +        Player.Velocity.Y = 200
//      end
//  end`

// 	diffObj, err := NewDiff(diffText, &sourceCode)
// 	if err != nil {
// 		t.Fatalf("Error parsing diff: %v", err)
// 	}

// 	if len(diffObj.Hunks) != 1 {
// 		t.Errorf("Hunks count is %d, expected 1", len(diffObj.Hunks))
// 	}

// 	if !diffObj.HunksAreValid() {
// 		t.Errorf("Diff is considered invalid")
// 	}

// 	if diffObj.Hunks[0].Header.OriginalRange.Start != 21 {
// 		t.Errorf("Original range start is %d, expected 21", diffObj.Hunks[0].Header.OriginalRange.Start)
// 	}

// 	if diffObj.Hunks[0].Header.OriginalRange.Count != 6 {
// 		t.Errorf("Original range count is %d, expected 6", diffObj.Hunks[0].Header.OriginalRange.Count)
// 	}

// 	if diffObj.Hunks[0].Header.NewRange.Start != 21 {
// 		t.Errorf("New range start is %d, expected 21", diffObj.Hunks[0].Header.NewRange.Start)
// 	}

// 	if diffObj.Hunks[0].Header.NewRange.Count != 6 {
// 		t.Errorf("New range count is %d, expected 6", diffObj.Hunks[0].Header.NewRange.Count)
// 	}

// 	if diffObj.Hunks[0].Body.AddedLinesCount() != 1 {
// 		t.Errorf("Added lines count is %d, expected 1", diffObj.Hunks[0].Body.AddedLinesCount())
// 	}

// 	if diffObj.Hunks[0].Body.RemovedLinesCount() != 1 {
// 		t.Errorf("Removed lines count is %d, expected 1", diffObj.Hunks[0].Body.RemovedLinesCount())
// 	}

// 	if diffObj.Hunks[0].Body.UnchangedLinesCount() != 5 {
// 		t.Errorf("Unchanged lines count is %d, expected 5", diffObj.Hunks[0].Body.UnchangedLinesCount())
// 	}
// }

func TestParseDiff2(t *testing.T) {

	diffText := "```diff" + "\n" +
		"@@ -18,7 +18,7 @@" + "\n" +
		" -- (space bar on PC, button 1 on mobile)" + "\n" +
		" Client.Action1 = function()" + "\n" +
		"     if Player.IsOnGround then" + "\n" +
		"-        Player.Velocity.Y = 100" + "\n" +
		"+        Player.Velocity.Y = 200" + "\n" +
		"     end" + "\n" +
		" end" + "\n" +
		" " + "\n" +
		"```"

	sourceCode := `Config = {
    Map = "aduermael.hills",
}

-- Client.OnStart is the first function to be called when the world is launched, on each user's device.
Client.OnStart = function()

    -- This function drops the local player above the center of the map:
    dropPlayer = function()
        Player.Position = Number3(Map.Width * 0.5, Map.Height + 10, Map.Depth * 0.5) * Map.Scale
        Player.Rotation = { 0, 0, 0 }
        Player.Velocity = { 0, 0, 0 }
    end

    -- Add player to the World (root scene Object) and call dropPlayer().
    World:AddChild(Player)
    dropPlayer()
end

-- jump function, triggered with Action1
-- (space bar on PC, button 1 on mobile)
Client.Action1 = function()
    if Player.IsOnGround then
        Player.Velocity.Y = 100
    end
end
`

	diffObj, err := NewDiff(diffText, &sourceCode)
	if err != nil {
		t.Fatalf("Error parsing diff: %v", err)
	}

	if len(diffObj.Hunks) != 1 {
		t.Errorf("Hunks count is %d, expected 1", len(diffObj.Hunks))
	}

	err = diffObj.FixHunks()
	if err != nil {
		t.Errorf("Error fixing diff: %v", err)
	}
}

// const VALID_DIFF_4 = `--- diffs_valid/4/file1.txt	2025-03-20 10:28:03
// +++ diffs_valid/4/file2.txt	2025-03-20 10:28:46
// @@ -11,11 +11,6 @@
//  	codeLines := strings.Split(luauCode, "\n")
//  	diffLines := strings.Split(unifiedDiff, "\n")

// -	// Basic validation of diff format
// -	if len(diffLines) < 3 { // Minimum: header + hunk header + one change
// -		return false
// -	}
// -
//  	// Process each line of the diff
//  	var currentLine int
//  	for i, line := range diffLines {
// @@ -70,6 +65,8 @@
//  			continue
//  		}

// +        fmt.Println("Hello there....")
// +
//  		// Validate context and removal lines match the original code
//  		if strings.HasPrefix(line, " ") || strings.HasPrefix(line, "-") {
//  			if currentLine >= len(codeLines) {
// @@ -86,3 +83,7 @@

//  	return true
//  }
// +
// +func foo() {
// +    fmt.Println("Hello there....")
// +}`

// // TestParseAllHunks parses all hunks from a unified diff
// func TestParseAllHunks(t *testing.T) {
// 	unifiedDiff := VALID_DIFF_4

// 	hunks, err := ParseAllHunks(unifiedDiff)
// 	if err != nil {
// 		t.Fatalf("Error parsing all hunks: %v", err)
// 	}
// 	if len(hunks) != 3 {
// 		t.Errorf("Expected 3 hunks, got %d", len(hunks))
// 	}
// }

// const VALID_HUNK_1 = `@@ -11,11 +11,6 @@
// codeLines := strings.Split(luauCode, "\n")
// diffLines := strings.Split(unifiedDiff, "\n")

// -	// Basic validation of diff format
// -	if len(diffLines) < 3 { // Minimum: header + hunk header + one change
// -		return false
// -	}
// -
// // Process each line of the diff
// var currentLine int
// for i, line := range diffLines {`

// func TestParseSingleHunk(t *testing.T) {
// 	hunk, err := ParseHunk(VALID_HUNK_1)
// 	if err != nil {
// 		t.Fatalf("Error parsing hunk: %v", err)
// 	}

// 	if hunk.Header.OriginalRange.Start != 11 {
// 		t.Errorf("Expected original range start to be 11, got %d", hunk.Header.OriginalRange.Start)
// 	}

// 	if hunk.Header.OriginalRange.Count != 11 {
// 		t.Errorf("Expected original range count to be 11, got %d", hunk.Header.OriginalRange.Count)
// 	}

// 	if hunk.Header.NewRange.Start != 11 {
// 		t.Errorf("Expected new range start to be 11, got %d", hunk.Header.NewRange.Start)
// 	}

// 	if hunk.Header.NewRange.Count != 6 {
// 		t.Errorf("Expected new range count to be 6, got %d", hunk.Header.NewRange.Count)
// 	}

// 	if hunk.AddedLinesCount != 0 {
// 		t.Errorf("Expected added lines to be 0, got %d", hunk.AddedLinesCount)
// 	}

// 	if hunk.RemovedLinesCount != 5 {
// 		t.Errorf("Expected removed lines to be 5, got %d", hunk.RemovedLinesCount)
// 	}

// 	if hunk.UnchangedLinesCount != 6 {
// 		t.Errorf("Expected unchanged lines to be 6, got %d", hunk.UnchangedLinesCount)
// 	}

// 	if hunk.Header.OriginalRange.Count != uint(hunk.UnchangedLinesCount)+uint(hunk.RemovedLinesCount) {
// 		t.Errorf("Expected original range count to be unchanged lines + removed lines, got %d", hunk.Header.OriginalRange.Count)
// 	}

// 	if hunk.Header.NewRange.Count != uint(hunk.UnchangedLinesCount)+uint(hunk.AddedLinesCount) {
// 		t.Errorf("Expected new range count to be unchanged lines + added lines, got %d", hunk.Header.NewRange.Count)
// 	}
// }

// //

// func TestIsDiffValid(t *testing.T) {
// 	dirs, err := os.ReadDir("diffs_valid")
// 	if err != nil {
// 		t.Fatalf("Error reading diffs_valid directory: %v", err)
// 	}

// 	for _, dir := range dirs {
// 		if !dir.IsDir() {
// 			continue
// 		}

// 		dirPath := fmt.Sprintf("diffs_valid/%s", dir.Name())
// 		file1Path := fmt.Sprintf("%s/file1.txt", dirPath)
// 		diffPath := fmt.Sprintf("%s/diff.txt", dirPath)

// 		// Read file contents
// 		file1Content, err := os.ReadFile(file1Path)
// 		if err != nil {
// 			t.Fatalf("Error reading file1.txt in %s: %v", dirPath, err)
// 		}

// 		diffContent, err := os.ReadFile(diffPath)
// 		if err != nil {
// 			t.Fatalf("Error reading diff.txt in %s: %v", dirPath, err)
// 		}

// 		// Test the function
// 		result := isDiffValidForCode(string(file1Content), string(diffContent))
// 		if !result {
// 			t.Errorf("isDiffValidForCode returned false for directory %s, expected true", dir.Name())
// 		}
// 	}
// }

// func TestIsDiffInvalid(t *testing.T) {
// 	dirs, err := os.ReadDir("diffs_invalid")
// 	if err != nil {
// 		t.Fatalf("Error reading diffs_invalid directory: %v", err)
// 	}

// 	for _, dir := range dirs {
// 		if !dir.IsDir() {
// 			continue
// 		}

// 		dirPath := fmt.Sprintf("diffs_invalid/%s", dir.Name())
// 		file1Path := fmt.Sprintf("%s/file1.txt", dirPath)
// 		diffPath := fmt.Sprintf("%s/diff.txt", dirPath)

// 		file1Content, err := os.ReadFile(file1Path)
// 		if err != nil {
// 			t.Fatalf("Error reading file1.txt from %s: %v", dirPath, err)
// 		}

// 		diffContent, err := os.ReadFile(diffPath)
// 		if err != nil {
// 			t.Fatalf("Error reading diff.txt from %s: %v", dirPath, err)
// 		}

// 		if isDiffValidForCode(string(file1Content), string(diffContent)) {
// 			t.Errorf("Expected invalid diff for %s but it was reported as valid", dirPath)
// 		}
// 	}
// }

func TestGeneratedDiff1(t *testing.T) {
	const DIR_NUMBER = 1

	dirPath := filepath.Join("diffs_generated", fmt.Sprintf("%d", DIR_NUMBER))
	genDiffPath := filepath.Join(dirPath, "generated_diff.txt")
	sourcePath := filepath.Join(dirPath, "source.txt")
	wantedPath := filepath.Join(dirPath, "wanted.txt")

	// Read the generated diff
	genDiffText, err := os.ReadFile(genDiffPath)
	if err != nil {
		t.Fatalf("Error reading generated_diff.txt from %s: %v", dirPath, err)
	}

	// Read the original source code
	sourceContent, err := os.ReadFile(sourcePath)
	if err != nil {
		t.Fatalf("Error reading source.txt from %s: %v", dirPath, err)
	}
	sourceContentStr := string(sourceContent)

	// Parse the generated diff
	genDiff, err := NewDiff(string(genDiffText), &sourceContentStr)
	if err != nil {
		t.Fatalf("Error parsing generated_diff.txt from %s: %v", dirPath, err)
	}

	// This diff must be considered valid by itself
	if genDiff.HunksAreValid() == false {
		t.Errorf("Generated diff for %s has its hunks considered invalid (but they are in fact valid)", dirPath)
	}

	// This diff must be applicable to the code
	sourceContentLines := ParseLines(sourceContentStr)
	err = genDiff.IsApplicableToCode(sourceContentLines)
	if err != nil {
		t.Errorf("generated diff was not fixed while being parsed: %s", dirPath)
	}

	err = genDiff.FixHunks()
	if err != nil {
		t.Errorf("generated diff for %s has error [%v] while fixing it", dirPath, err)
	}
	// genDiff has been fixed

	// Load "wanted result" file
	wantedContent, err := os.ReadFile(wantedPath)
	if err != nil {
		t.Fatalf("Error reading wanted.txt from %s: %v", dirPath, err)
	}

	if genDiff.ToString() != string(wantedContent) {
		t.Errorf("Generated diff for %s does not match wanted diff", dirPath)
	}
}

func TestGeneratedDiff2(t *testing.T) {
	const DIR_NUMBER = 2

	dirPath := filepath.Join("diffs_generated", fmt.Sprintf("%d", DIR_NUMBER))
	genDiffPath := filepath.Join(dirPath, "generated_diff.txt")
	sourcePath := filepath.Join(dirPath, "source.txt")
	wantedPath := filepath.Join(dirPath, "wanted.txt")

	// Read the generated diff
	genDiffText, err := os.ReadFile(genDiffPath)
	if err != nil {
		t.Fatalf("Error reading generated_diff.txt from %s: %v", dirPath, err)
	}

	// Read the original source code
	sourceContent, err := os.ReadFile(sourcePath)
	if err != nil {
		t.Fatalf("Error reading source.txt from %s: %v", dirPath, err)
	}
	sourceContentStr := string(sourceContent)

	// Parse the generated diff
	genDiff, err := NewDiff(string(genDiffText), &sourceContentStr)
	if err != nil {
		t.Fatalf("Error parsing generated_diff.txt from %s: %v", dirPath, err)
	}

	// This diff must be applicable to the code
	sourceContentLines := ParseLines(string(sourceContent))
	err = genDiff.IsApplicableToCode(sourceContentLines)
	if err != nil {
		t.Errorf("generated diff was not fixed while being parsed: %s", dirPath)
	}

	err = genDiff.FixHunks()
	if err != nil {
		t.Errorf("generated diff for %s has error [%v] while fixing it", dirPath, err)
	}
	// genDiff has been fixed

	// Load "wanted result" file
	wantedContent, err := os.ReadFile(wantedPath)
	if err != nil {
		t.Fatalf("Error reading wanted.txt from %s: %v", dirPath, err)
	}

	if genDiff.ToString() != string(wantedContent) {
		fmt.Printf("⭐️ Generated diff:\n%s\n⭐️\n", genDiff.ToString())
		fmt.Printf("⭐️ Wanted diff:\n%s\n⭐️\n", string(wantedContent))
		t.Errorf("Generated diff for %s does not match wanted diff", dirPath)
	}
}

func TestGeneratedDiff3(t *testing.T) {
	const DIR_NUMBER = 3

	dirPath := filepath.Join("diffs_generated", fmt.Sprintf("%d", DIR_NUMBER))
	genDiffPath := filepath.Join(dirPath, "generated_diff.txt")
	sourcePath := filepath.Join(dirPath, "source.txt")
	wantedPath := filepath.Join(dirPath, "wanted.txt")

	// Read the generated diff
	genDiffText, err := os.ReadFile(genDiffPath)
	if err != nil {
		t.Fatalf("Error reading generated_diff.txt from %s: %v", dirPath, err)
	}

	// Read the original source code
	sourceContent, err := os.ReadFile(sourcePath)
	if err != nil {
		t.Fatalf("Error reading source.txt from %s: %v", dirPath, err)
	}
	sourceContentStr := string(sourceContent)

	// Parse the generated diff
	genDiff, err := NewDiff(string(genDiffText), &sourceContentStr)
	if err != nil {
		t.Fatalf("Error parsing generated_diff.txt from %s: %v", dirPath, err)
	}

	// This diff must be applicable to the code
	sourceContentLines := ParseLines(string(sourceContent))
	err = genDiff.IsApplicableToCode(sourceContentLines)
	if err != nil {
		t.Errorf("generated diff was not fixed while being parsed: %s", dirPath)
	}

	err = genDiff.FixHunks()
	if err != nil {
		t.Fatalf("generated diff for %s has error [%v] while fixing it", dirPath, err)
	}
	// genDiff has been fixed
	// Load "wanted result" file
	wantedContent, err := os.ReadFile(wantedPath)
	if err != nil {
		t.Fatalf("Error reading wanted.txt from %s: %v", dirPath, err)
	}

	if genDiff.ToString() != string(wantedContent) {
		t.Errorf("Generated diff for %s does not match wanted diff", dirPath)
	}
}
