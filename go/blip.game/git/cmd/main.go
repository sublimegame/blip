package main

import (
	"log"
	"os"

	"github.com/go-git/go-git/v5/plumbing"

	"blip.game/git"
)

func main() {

	// Get first argument
	if len(os.Args) < 2 {
		log.Fatal("Repository path is required")
	}
	repoPath := os.Args[1]
	if repoPath == "" {
		log.Fatal("Repository path is required")
	}

	// Open repository
	repo, err := git.OpenRepo(repoPath)
	if err != nil {
		log.Fatal(err)
	}

	// Reset to commit
	err = repo.Reset(plumbing.NewHash("f31f297ee6f04a040d3ecfd69c791295a9cd2ff6"))
	if err != nil {
		log.Fatal(err)
	}

	// // Get status
	// status, err := repo.Status()
	// if err != nil {
	// 	log.Fatal(err)
	// }

	// fmt.Println(status)

	// for key, file := range status {
	// 	fmt.Println(key, file.Extra, file.Staging, file.Worktree)
	// 	hash, err := repo.Add(key)
	// 	if err != nil {
	// 		log.Fatal(err)
	// 	}
	// }

	// fmt.Println("🛠️ Commiting...")
	// opts := &git.CommitOptions{
	// 	Author: &object.Signature{
	// 		Name:  "Blip Game",
	// 		Email: "blip@blip.game",
	// 		When:  time.Now(),
	// 	},
	// }
	// commit, err := repo.Commit("Add smiley", opts)
	// if err != nil {
	// 	log.Fatal(err)
	// }
}
