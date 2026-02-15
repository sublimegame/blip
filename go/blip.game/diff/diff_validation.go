package diff

// func isDiffValidForCode(codeText string, diffText string) bool {

// 	// parse diff

// 	diff, err := NewDiff(diffText)
// 	if err != nil {
// 		fmt.Println("failed to parse diff:", err.Error())
// 		return false
// 	}

// 	// first, validate diff by itself
// 	// Note: we don't validate the header here, because it is not always present
// 	// in the AI generated diffs and we don't really care about it

// 	if !diff.HunksAreValid() {
// 		return false
// 	}

// 	// then validate diff against code

// 	applicabilityResult := diff.IsApplicableToCode(codeText)

// 	if !applicabilityResult.IsApplicable() {
// 		return false
// 	}

// 	return true
// }

// // find line of first hunk header
// func getLineNumberOfFirstHunkHeader(unifiedDiff string) (int, error) {
// 	scanner := bufio.NewScanner(strings.NewReader(unifiedDiff))
// 	lineCounter := 0
// 	found := false
// 	for scanner.Scan() {
// 		lineCounter++
// 		if strings.HasPrefix(scanner.Text(), "@@") {
// 			found = true
// 			break
// 		}
// 	}
// 	if err := scanner.Err(); err != nil {
// 		return 0, err
// 	}
// 	if !found {
// 		return 0, fmt.Errorf("no hunk header found in diff")
// 	}
// 	return lineCounter, nil
// }

// func trimTextBeforeFirstHunkHeader(unifiedDiff string) (string, error) {
// 	lineNumber, err := getLineNumberOfFirstHunkHeader(unifiedDiff)
// 	if err != nil {
// 		return "", err
// 	}
// 	// line number cannot be 0 nor negative
// 	if lineNumber <= 0 {
// 		return "", errors.New("invalid line number")
// 	}
// 	// for each line until lineNumber, trim the first line
// 	for i := 0; i < lineNumber-1; i++ {
// 		unifiedDiff = strings.SplitN(unifiedDiff, "\n", 2)[1]
// 	}
// 	return unifiedDiff, nil
// }

// // validateHunkHeader checks if a hunk header in a unified diff is valid
// // by verifying the line counts match the actual changes.
// func validateHunkHeader(unifiedDiff string) bool {
// 	diffLines := strings.Split(unifiedDiff, "\n")

// 	for i, line := range diffLines {
// 		if strings.HasPrefix(line, "@@") {
// 			parts := strings.Split(line, " ")
// 			if len(parts) < 3 {
// 				return false
// 			}

// 			// Parse original file range: -start,count
// 			origRange := strings.TrimPrefix(parts[1], "-")
// 			origParts := strings.Split(origRange, ",")
// 			if len(origParts) != 2 {
// 				return false
// 			}

// 			// Count the actual lines being removed
// 			removedLines := 0
// 			contextLines := 0
// 			for j := i + 1; j < len(diffLines) && !strings.HasPrefix(diffLines[j], "@@"); j++ {
// 				if strings.HasPrefix(diffLines[j], "-") {
// 					removedLines++
// 				} else if strings.HasPrefix(diffLines[j], " ") {
// 					contextLines++
// 				}
// 			}

// 			// Parse the count from hunk header
// 			count, err := strconv.Atoi(origParts[1])
// 			if err != nil {
// 				return false
// 			}

// 			// Verify the count matches actual removed + context lines
// 			if count != removedLines+contextLines {
// 				return false
// 			}
// 		}
// 	}
// 	return true
// }
