package git

import (
	"crypto/sha1"
	"errors"
	"time"

	"github.com/go-git/go-git/v5"
	"github.com/go-git/go-git/v5/plumbing"
	"github.com/go-git/go-git/v5/plumbing/object"
)

const (
	DEFAULT_AUTHOR_NAME  = "default"
	DEFAULT_AUTHOR_EMAIL = "default@default.com"
)

var (
	// Expose some git errors
	ErrRepositoryNotExists = git.ErrRepositoryNotExists
	ErrEmptyCommit         = git.ErrEmptyCommit
)

type Repo struct {
	repo *git.Repository
}

// InitRepo initializes a new git repo at the given directory path
func InitRepo(dirPath string) (*Repo, error) {
	repo, err := git.PlainInit(dirPath, false)
	if err != nil {
		return nil, err
	}
	if repo == nil {
		return nil, errors.New("failed to initialize repository")
	}
	return &Repo{repo: repo}, nil
}

// OpenRepo opens an existing git repo
func OpenRepo(path string) (*Repo, error) {
	repo, err := git.PlainOpen(path)
	if err != nil {
		return nil, err
	}
	if repo == nil {
		return nil, errors.New("failed to open repository")
	}
	return &Repo{repo: repo}, nil
}

// Status returns the status of the repository
func (b *Repo) Status() (git.Status, error) {
	wt, err := b.repo.Worktree()
	if err != nil {
		return nil, err
	}
	status, err := wt.Status()
	if err != nil {
		return nil, err
	}
	return status, nil
}

// Add adds a file to the staging area
func (b *Repo) Add(path string) error {
	wt, err := b.repo.Worktree()
	if err != nil {
		return err
	}
	_, err = wt.Add(path)
	return err
}

// AddAll adds all files to the staging area
func (b *Repo) AddAll() error {
	wt, err := b.repo.Worktree()
	if err != nil {
		return err
	}
	return wt.AddWithOptions(&git.AddOptions{All: true})
}

// Commit creates a new commit with the given message and options
func (b *Repo) Commit(message, authorName, authorEmail string, when time.Time, opts CommitOpts) (Hash, error) {
	if authorName == "" {
		authorName = DEFAULT_AUTHOR_NAME
	}
	if authorEmail == "" {
		authorEmail = DEFAULT_AUTHOR_EMAIL
	}
	if when.IsZero() {
		when = time.Now()
	}

	wt, err := b.repo.Worktree()
	if err != nil {
		return Hash{}, err
	}

	commitOpts := git.CommitOptions{
		All:               opts.All,
		AllowEmptyCommits: opts.AllowEmptyCommits,
		Author:            &object.Signature{Name: authorName, Email: authorEmail, When: when},
	}

	return wt.Commit(message, &commitOpts)
}

// Reset resets the repository to the given commit
func (b *Repo) Reset(commitHash Hash) error {
	wt, err := b.repo.Worktree()
	if err != nil {
		return err
	}
	return wt.Reset(&git.ResetOptions{
		Commit: commitHash,
		Mode:   git.HardReset,
	})
}

func (b *Repo) GetLastCommitAndCode(codeGitFilename string) (Commit, string, error) {

	ref, err := b.repo.Head()
	if err != nil {
		return Commit{}, "", err
	}

	commitObj, err := b.repo.CommitObject(ref.Hash())
	if err != nil {
		return Commit{}, "", err
	}

	// get content of code file
	tree, err := commitObj.Tree()
	if err != nil {
		return Commit{}, "", err
	}
	file, err := tree.File(codeGitFilename)
	if err != nil {
		return Commit{}, "", err
	}
	fileContent, err := file.Contents()
	if err != nil {
		return Commit{}, "", err
	}

	return Commit{
		Hash:    ref.Hash().String(),
		Message: commitObj.Message,
	}, fileContent, nil
}

type GetCommitHistoryOpts struct {
	Limit int
}

func (b *Repo) GetCommitHistory(opts GetCommitHistoryOpts) ([]Commit, error) {
	if opts.Limit == 0 {
		opts.Limit = 30
	}

	commits := make([]Commit, 0)

	iter, err := b.repo.Log(&git.LogOptions{})
	if err != nil {
		return nil, err
	}

	for {
		commit, err := iter.Next()
		if err != nil || commit == nil || len(commits) >= opts.Limit {
			iter.Close()
			break
		}
		commits = append(commits, Commit{
			Hash:      commit.Hash.String(),
			Message:   commit.Message,
			CreatedAt: commit.Author.When,
		})
	}

	return commits, nil
}

func (b *Repo) ResetHard(commitHashStr string) error {
	// validate the commit hash is a SHA1 hex string
	if len(commitHashStr) != sha1.Size*2 {
		return errors.New("invalid commit hash")
	}

	// convert the commit hash to a plumbing.Hash
	hash := plumbing.NewHash(commitHashStr)

	wt, err := b.repo.Worktree()
	if err != nil {
		return err
	}
	return wt.Reset(&git.ResetOptions{
		Commit: hash,
		Mode:   git.HardReset,
	})
}
