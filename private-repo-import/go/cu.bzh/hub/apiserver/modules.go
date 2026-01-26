package main

import (
	"crypto/sha256"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"cu.bzh/hub/scyllaclient"
)

const (
	GITHUB_PREFIX = "github.com"
)

type Module struct {
	Commit string
	Script string
}

func fetchModule(url, etag string) (*Module, error) {
	fmt.Println("fetch module:", url)

	var module *Module
	var err error

	if strings.HasPrefix(url, GITHUB_PREFIX) {
		module, err = pullGithubModule(url)
		if err != nil {
			return nil, err
		}
	} else {
		return nil, errors.New("only modules from github.com supported so far")
	}

	return module, nil
}

type GithubModuleInfo struct {
	Host      string
	RepoOwner string
	RepoName  string
	Path      string
	Ref       string
}

func (mi *GithubModuleInfo) Origin() string {
	return mi.Host + "/" + mi.RepoOwner + "/" + mi.RepoName + "/" + mi.Path
}

type GithubDirectory []*GithubFile

type GithubFile struct {
	Name        string `json:"name"`
	DownloadURL string `json:"download_url"`
}

func parseGithubModuleInfo(url string) (*GithubModuleInfo, error) {
	parts := strings.Split(url, "/")

	if len(parts) < 3 {
		return nil, errors.New("incorrect Github module URL: " + url)
	}

	lastPart := parts[len(parts)-1]
	lastPartAndRef := strings.Split(lastPart, ":")
	ref := ""

	if len(lastPartAndRef) > 1 {
		parts[len(parts)-1] = lastPartAndRef[0]
		ref = lastPartAndRef[1]
	}

	result := &GithubModuleInfo{
		Host:      "",
		RepoOwner: "",
		RepoName:  "",
		Path:      "",
		Ref:       ref,
	}

	result.Host = parts[0]

	result.RepoOwner = parts[1]
	result.RepoName = parts[2]

	if len(parts) > 3 {
		result.Path = strings.Join(parts[3:], "/")
	}

	return result, nil
}

func computeRefHash(origin, ref, script string) string {
	hash := sha256.New()
	hash.Write([]byte(origin))
	hash.Write([]byte(ref))
	hash.Write([]byte(script))
	hash.Write([]byte("Fautpasrespirerlacompote,çafaittousser._")) // add salt
	hashed := hash.Sum(nil)
	// convert the hashed result to a hexadecimal string
	return fmt.Sprintf("%x", hashed)
}

func pullGithubModule(url string) (*Module, error) {
	fmt.Println("🐙 === PULL GITHUB MODULE", url)

	moduleInfo, err := parseGithubModuleInfo(url)
	if err != nil {
		return nil, err
	}

	// Lookup in cache for the module
	// Note: the module might be in cache, but not the ref
	{
		mod, err := scyllaClientUniverse.GetUniverseModuleByRef(moduleInfo.Origin(), moduleInfo.Ref)
		if err != nil {
			return nil, err
		}
		if mod != nil {
			fmt.Println("🐙 module found in cache! ✅")
			return &Module{
				Script: mod.Script,
				Commit: mod.Commit,
			}, nil
		} else {
			fmt.Println("🐙 module not found in cache...")
		}
	}

	// Lookup the Ref using the GitHub API and obtain the corresponding commit SHA

	type RefType int
	const (
		RefTypeTag RefType = iota
		RefTypeBranch
		RefTypeCommit
	)

	// NOTE: moduleInfo.Ref can be ""
	// if moduleInfo.Ref == "" {
	// 	moduleInfo.Ref = ".latest" // convention for module "latest version"
	// }

	fmt.Println("🐙 resolving github ref:", moduleInfo.Ref)

	// obtain commit hash corresponding to the ref
	// ref check priority : tag > branch > commit
	var commitSha string
	var commitTime time.Time
	var refType RefType
	{
		refToUse := moduleInfo.Ref
		skipTagCheck := false
		skipBranchCheck := false

		// if moduleInfo.Ref == ".latest" {
		if moduleInfo.Ref == "" {
			refToUse = "HEAD"
			skipTagCheck = true
			skipBranchCheck = true
		}

		// check if ref is a tag
		if commitSha == "" && !skipTagCheck {

			// get the ref type (tag, branch, commit)
			requestURL := "https://api.github.com/repos/" + moduleInfo.RepoOwner + "/" + moduleInfo.RepoName + "/git/refs/tags/" + refToUse
			resp, err := http.Get(requestURL)
			if err != nil {
				return nil, err
			}
			defer resp.Body.Close()

			if resp.StatusCode == http.StatusOK {
				decoder := json.NewDecoder(resp.Body)

				var ref map[string]interface{}
				if err := decoder.Decode(&ref); err != nil {
					return nil, err
				}

				tagUrl := ref["object"].(map[string]interface{})["url"].(string)

				resp, err := http.Get(tagUrl)
				if err != nil {
					return nil, err
				}
				defer resp.Body.Close()

				if resp.StatusCode == http.StatusOK {
					decoder := json.NewDecoder(resp.Body)

					var tag map[string]interface{}
					if err := decoder.Decode(&tag); err != nil {
						return nil, err
					}

					objType := tag["object"].(map[string]interface{})["type"].(string)
					if objType != "commit" {
						return nil, errors.New("unexpected type. Expected commit, got " + objType)
					}

					commitUrl := tag["object"].(map[string]interface{})["url"].(string)

					resp, err := http.Get(commitUrl)
					if err != nil {
						return nil, err
					}
					defer resp.Body.Close()

					if resp.StatusCode == http.StatusOK {
						decoder := json.NewDecoder(resp.Body)

						var respObj map[string]interface{}
						if err := decoder.Decode(&respObj); err != nil {
							return nil, err
						}

						commitSha = respObj["sha"].(string)
						commitTime, err = time.Parse(time.RFC3339, respObj["commit"].(map[string]interface{})["committer"].(map[string]interface{})["date"].(string))
						if err != nil {
							return nil, err
						}
						refType = RefTypeTag
					}
				}
			}
		}

		// check if ref is a branch
		if commitSha == "" && !skipBranchCheck {
			requestURL := "https://api.github.com/repos/" + moduleInfo.RepoOwner + "/" + moduleInfo.RepoName + "/git/refs/heads/" + refToUse
			resp, err := http.Get(requestURL)
			if err != nil {
				return nil, err
			}
			defer resp.Body.Close()

			if resp.StatusCode == http.StatusOK {
				decoder := json.NewDecoder(resp.Body)

				var branch map[string]interface{}
				if err := decoder.Decode(&branch); err != nil {
					return nil, err
				}

				object := branch["object"].(map[string]interface{})

				objType := object["type"].(string)
				if objType != "commit" {
					return nil, errors.New("unexpected type. Expected commit, got " + objType)
				}

				commitUrl := object["url"].(string)

				resp, err := http.Get(commitUrl)
				if err != nil {
					return nil, err
				}
				defer resp.Body.Close()

				if resp.StatusCode == http.StatusOK {
					decoder := json.NewDecoder(resp.Body)

					var respObj map[string]interface{}
					if err := decoder.Decode(&respObj); err != nil {
						return nil, err
					}

					commitSha = respObj["sha"].(string)
					commitTime, err = time.Parse(time.RFC3339, respObj["committer"].(map[string]interface{})["date"].(string))
					if err != nil {
						return nil, err
					}
					refType = RefTypeBranch
				}
			}
		}

		// if ref was not tag nor branch, check for commit
		if commitSha == "" {
			// get the latest commit
			requestURL := "https://api.github.com/repos/" + moduleInfo.RepoOwner + "/" + moduleInfo.RepoName + "/commits/" + refToUse
			resp, err := http.Get(requestURL)
			if err != nil {
				return nil, err
			}
			defer resp.Body.Close()
			if resp.StatusCode != http.StatusOK {
				return nil, errors.New("HTTP request failed")
			}
			decoder := json.NewDecoder(resp.Body)
			var respObj map[string]interface{}
			if err := decoder.Decode(&respObj); err != nil {
				return nil, err
			}

			commitSha = respObj["sha"].(string)
			commitTime, err = time.Parse(time.RFC3339, respObj["commit"].(map[string]interface{})["committer"].(map[string]interface{})["date"].(string))
			if err != nil {
				return nil, err
			}
			refType = RefTypeCommit
		}
	}

	fmt.Println("🐙 commit sha :", commitSha)
	fmt.Println("🐙 commit time:", commitTime)

	// Now that we have the commit SHA, we can check again in cache to see if the module is already there
	{
		origin := moduleInfo.Origin()
		mod, err := scyllaClientUniverse.GetUniverseModule(origin, commitSha)
		if err != nil {
			return nil, err
		}

		if mod != nil {
			// module exists, but not the ref, we need to add the ref
			fmt.Println("🐙 module found in cache for diff ref! Adding new ref... ✅")

			ref := scyllaclient.UniverseModuleRefRecord{
				Origin:      origin,
				Ref:         moduleInfo.Ref,
				Commit:      commitSha,
				RefIsCommit: refType == RefTypeCommit,
				Hash:        computeRefHash(origin, moduleInfo.Ref, mod.Script),
				CreatedAt:   time.Now(),
				UpdatedAt:   time.Now(),
			}

			err := scyllaClientUniverse.InsertUniverseModuleRef(ref)
			if err != nil {
				return nil, err
			}

			return &Module{
				Script: mod.Script,
				Commit: mod.Commit,
			}, nil
		} else {
			fmt.Println("🐙 module not found in cache... (for diff ref)")
		}
	}

	fmt.Println("🐙 downloading content...")

	requestURL := "https://api.github.com/repos/" + moduleInfo.RepoOwner + "/" + moduleInfo.RepoName + "/contents"

	if moduleInfo.Path != "" {
		requestURL += "/" + moduleInfo.Path
	}

	if commitSha != "" {
		requestURL += "?ref=" + commitSha
	}

	resp, err := http.Get(requestURL)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, errors.New("HTTP request failed")
	}

	decoder := json.NewDecoder(resp.Body)

	var dir GithubDirectory
	if err := decoder.Decode(&dir); err != nil {
		return nil, err
	}

	combinedScript := ""

	for _, file := range dir {
		if strings.HasSuffix(file.Name, ".lua") {

			resp, err := http.Get(file.DownloadURL)
			if err != nil {
				return nil, err
			}
			defer resp.Body.Close()

			if resp.StatusCode != http.StatusOK {
				return nil, errors.New("HTTP request failed")
			}

			body, err := io.ReadAll(resp.Body)
			if err != nil {
				return nil, errors.New("Error reading response body")
			}

			if combinedScript != "" {
				combinedScript += "\n"
			}
			combinedScript += string(body)
		}
	}

	combinedScript = strings.TrimSpace(combinedScript)

	// save the module in the cache
	{
		fmt.Println("🐙 save module & ref in cache...")
		origin := moduleInfo.Origin()
		now := time.Now()

		mod := scyllaclient.UniverseModuleRecord{
			Origin:          origin,
			Commit:          commitSha,
			Script:          combinedScript,
			Documentation:   "",
			CommitCreatedAt: commitTime,
			CreatedAt:       now,
			UpdatedAt:       now,
		}

		refs := []scyllaclient.UniverseModuleRefRecord{
			{
				Origin:      origin,
				Ref:         moduleInfo.Ref,
				Commit:      commitSha,
				RefIsCommit: refType == RefTypeCommit,
				Hash:        computeRefHash(origin, moduleInfo.Ref, mod.Script),
				CreatedAt:   now,
				UpdatedAt:   now,
			},
		}

		err := scyllaClientUniverse.InsertUniverseModule(mod, refs, nil)
		if err != nil {
			return nil, err
		}
	}

	fmt.Println("🐙 done ✅")
	return &Module{
		Script: combinedScript,
		Commit: commitSha,
	}, nil
}
