package main

import (
	"bufio"
	"fmt"
	"math/rand"
	"os"
	"strings"
	"time"
)

var FILES []string = []string{
	"items-hair.txt",
	"items-jacket.txt",
	"items-boot.txt",
}

func processFile(filename string) ([]string, error) {
	items := make([]string, 0)

	// Open the file
	file, err := os.Open(filename)
	if err != nil {
		return items, err
	}
	defer file.Close()

	// Create a new scanner to read the file line by line
	scanner := bufio.NewScanner(file)

	// Loop through each line
	for scanner.Scan() {
		line := scanner.Text()
		// Split the line by " -> " to separate the description and the unique ID
		parts := strings.Split(line, " -> ")
		if len(parts) == 2 {
			// Print the unique ID
			ID := parts[1]
			// fmt.Println(ID)
			items = append(items, ID)
		}
	}

	// Check for errors during scanning
	if err := scanner.Err(); err != nil {
		return items, err
	}

	return items, nil
}

// Function to shuffle a slice of strings
func shuffleStrings(slice []string) {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))
	r.Shuffle(len(slice), func(i, j int) {
		slice[i], slice[j] = slice[j], slice[i]
	})
}

func main() {
	itemsGlobal := make([]string, 0)

	for _, FILE := range FILES {
		items, err := processFile(FILE)
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
			return
		}
		itemsGlobal = append(itemsGlobal, items...)
	}

	shuffleStrings(itemsGlobal)

	for _, item := range itemsGlobal {
		fmt.Println(`"` + item + `",`)
	}
}
