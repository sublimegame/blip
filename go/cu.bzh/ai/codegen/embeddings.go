package main

import (
	"context"
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strings"

	"cu.bzh/ai"
	ollama "github.com/ollama/ollama/api"
	yaml "gopkg.in/yaml.v2"
)

var (
	pages map[string]*Page
	// 	pagesV2 map[string]*Module
)

func convertSampleScriptsIntoEmbeddingDocuments() error {
	if err := os.RemoveAll(SAMPLE_SCRIPTS_MARKDOWN_OUTPUT_DIR); err != nil {
		return fmt.Errorf("failed to remove output directory: %v", err)
	}

	if err := os.MkdirAll(SAMPLE_SCRIPTS_MARKDOWN_OUTPUT_DIR, 0755); err != nil {
		return fmt.Errorf("failed to create output directory: %v", err)
	}

	file, err := os.Open(sample_scripts_file)
	if err != nil {
		fmt.Println(err.Error())
		return fmt.Errorf("failed to open sample scripts file: %v", err)
	}
	defer file.Close()

	var page Page

	err = yaml.NewDecoder(file).Decode(&page)
	if err != nil {
		fmt.Println(err.Error())
		return fmt.Errorf("%s %v", sample_scripts_file, err)
	}

	page.Sanitize()

	fmt.Println(page)

	var currentPage *Page = nil
	currentTitle := ""

	writeCurrentPage := func(p *Page) {
		if p == nil {
			return
		}
		if len(p.Blocks) == 0 {
			return
		}
		md := generateMarkdownForSamplePage(p)
		if md == "" {
			return
		}
		fmt.Println(md)
		fmt.Println("-----")
		// Create filename from page type

		fileName := currentPage.Title
		if currentPage.Blocks[0].Subtitle != "" {
			if fileName != "" {
				fileName += "-"
			}
			fileName += currentPage.Blocks[0].Subtitle
		}

		// Replace sequences of \ -_ with a single -
		re := regexp.MustCompile(`[/\\_\s-]+`)
		fileName = re.ReplaceAllString(fileName, "-")

		filename := filepath.Join(SAMPLE_SCRIPTS_MARKDOWN_OUTPUT_DIR, fileName+".md")
		if err := os.WriteFile(filename, []byte(md), 0644); err != nil {
			fmt.Println("failed to write markdown file", filename, err)
			return
		}
	}

	for _, block := range page.Blocks {
		if block.Title != "" {
			writeCurrentPage(currentPage)
			currentTitle = block.Title
			currentPage = &Page{
				Keywords:       []string{},
				Description:    "",
				Title:          currentTitle,
				Type:           "",
				Blocks:         []*ContentBlock{},
				Constructors:   []*Function{},
				Functions:      []*Function{},
				Properties:     []*Property{},
				BaseProperties: map[string][]*Property{},
				BuiltIns:       []*Property{},
				BaseFunctions:  map[string][]*Function{},
				ResourcePath:   "",
			}
		} else if block.Subtitle != "" {
			writeCurrentPage(currentPage)
			currentPage = &Page{
				Keywords:       []string{},
				Description:    "",
				Title:          currentTitle,
				Type:           "",
				Blocks:         []*ContentBlock{block},
				Constructors:   []*Function{},
				Functions:      []*Function{},
				Properties:     []*Property{},
				BaseProperties: map[string][]*Property{},
				BuiltIns:       []*Property{},
				BaseFunctions:  map[string][]*Function{},
				ResourcePath:   "",
			}
		} else {
			if currentPage != nil {
				currentPage.Blocks = append(currentPage.Blocks, block)
			}
		}
	}

	// write last page when done parsing all blocks
	writeCurrentPage(currentPage)

	return nil
}

func convertReferenceIntoEmbeddingDocuments() error {
	pages = make(map[string]*Page)

	if err := os.RemoveAll(REFERENCE_MARKDOWN_OUTPUT_DIR); err != nil {
		return fmt.Errorf("failed to remove output directory: %v", err)
	}

	if err := os.MkdirAll(REFERENCE_MARKDOWN_OUTPUT_DIR, 0755); err != nil {
		return fmt.Errorf("failed to create output directory: %v", err)
	}

	dir := docs_reference_dir
	err := filepath.Walk(dir, func(path string, info os.FileInfo, err error) error {
		if !info.IsDir() && strings.HasSuffix(info.Name(), ".yml") {
			// check if path points to a regular file
			exists := regularFileExists(path)
			if exists {
				var page Page

				file, err := os.Open(path)
				if err != nil {
					return err
				}
				defer file.Close()

				// example: from /www/index.yml to /index.yml
				trimmedPath := strings.TrimPrefix(path, docs_reference_dir)
				page.ResourcePath = trimmedPath
				cleanPath := cleanPath(trimmedPath)

				fmt.Println(filepath.Join("/reference/", cleanPath))

				err = yaml.NewDecoder(file).Decode(&page)
				if err != nil {
					return fmt.Errorf("%s %v", trimmedPath, err)
				}
				pages[cleanPath] = &page
			}
		}
		return nil
	})
	if err != nil {
		return fmt.Errorf("error walking directory: %v", err)
	}

	for _, page := range pages {
		page.Sanitize()
	}

	// Convert pages to markdown and save them
	var fullReference strings.Builder
	for _, page := range pages {
		if page.Type == "" {
			continue
		}

		md := generateMarkdownForPage(page)
		if md == "" {
			continue
		}

		// Create filename from page type
		filename := filepath.Join(REFERENCE_MARKDOWN_OUTPUT_DIR, page.Type+".md")
		if err := os.WriteFile(filename, []byte(md), 0644); err != nil {
			return fmt.Errorf("failed to write markdown file %s: %v", filename, err)
		}

		// Add to full reference
		fullReference.WriteString("```md\n")
		fullReference.WriteString(md)
		fullReference.WriteString("\n```\n\n")
	}

	// Write the full reference file
	if err := os.WriteFile(FULL_REFERENCE_OUTPUT_FILE, []byte(fullReference.String()), 0644); err != nil {
		return fmt.Errorf("failed to write full reference file: %v", err)
	}

	return nil
}

func storeEmbeddings(ollamaClient *ollama.Client, chromaClient *ChromaClient) error {

	// // EMBED REFERENCE
	files, err := os.ReadDir(REFERENCE_MARKDOWN_OUTPUT_DIR)
	if err != nil {
		return fmt.Errorf("failed to read markdown directory: %v", err)
	}

	docsEmbedding, err := chromaClient.GetCollection("docs")
	if err != nil {
		return fmt.Errorf("failed to get collection: %v", err)
	}

	for _, file := range files {
		if !strings.HasSuffix(file.Name(), ".md") {
			continue
		}

		fPath := filepath.Join(REFERENCE_MARKDOWN_OUTPUT_DIR, file.Name())
		content, err := os.ReadFile(fPath)
		if err != nil {
			return fmt.Errorf("failed to read file %s: %v", file.Name(), err)
		}

		markdown := string(content)

		hash := md5.New()
		io.WriteString(hash, markdown)
		hashString := hex.EncodeToString(hash.Sum(nil))

		doc := Document{
			ID:       hashString,
			Markdown: markdown,
			Path:     fPath,
			Summary:  "",
		}

		fmt.Println("======================================")
		fmt.Println(fPath, "->", hashString)

		// summarize / normalize content before generating embedding

		messages := []ai.Message{
			{
				Role:    "user",
				Content: string(content),
			},
		}

		systemPrompt := ai.NewSystemPrompt()
		systemPrompt.Append(ai.NewTextPart("You're an agent that summarizes documentation pages."))
		systemPrompt.Append(ai.NewTextPart("Summaries should always be between 200 and 300 words."))
		systemPrompt.Append(ai.NewTextPart("Always consider requests to be a page that needs to be summarized, not a question."))
		systemPrompt.Append(ai.NewTextPart("Do not format your response, only raw text output is needed."))
		systemPrompt.Append(ai.NewTextPart("Include all important keywords (functions, properties) and concepts in the summary."))

		summary, err := askLLM(messages, askLLMOpts{
			ModelName:    MODEL_NAME_LOCAL, // we use the local model for embeddings
			SystemPrompt: systemPrompt,
		})
		if err != nil {
			return fmt.Errorf("[SUMMARIZER][ERROR]: %v", err)
		}

		doc.Summary = summary

		fmt.Println("```")
		fmt.Println(summary)
		fmt.Println("```")

		req := &ollama.EmbeddingRequest{
			Model:  MODEL_EMBEDDING_NAME,
			Prompt: summary,
		}

		resp, err := ollamaClient.Embeddings(context.Background(), req)
		if err != nil {
			fmt.Printf("❌ Failed to generate embedding for %s: %v\n", file.Name(), err)
			continue
		}

		fmt.Println("EMBEDDING:", resp.Embedding[:10])

		docJSON, err := json.Marshal(doc)
		if err != nil {
			return fmt.Errorf("failed to marshal document: %v", err)
		}

		err = docsEmbedding.Add([]ChromaCollectionEntry{
			{
				Embedding: &resp.Embedding,
				Document:  string(docJSON),
				ID:        hashString,
			},
		})
		if err != nil {
			return fmt.Errorf("failed to add embedding to collection: %v", err)
		}
	}

	// EMBED SAMPLE SCRIPTS

	files, err = os.ReadDir(SAMPLE_SCRIPTS_MARKDOWN_OUTPUT_DIR)
	if err != nil {
		return fmt.Errorf("failed to read markdown directory: %v", err)
	}

	sampleScriptsEmbedding, err := chromaClient.GetCollection("sampleScripts")
	if err != nil {
		return fmt.Errorf("failed to get collection: %v", err)
	}

	for _, file := range files {
		if !strings.HasSuffix(file.Name(), ".md") {
			continue
		}

		fPath := filepath.Join(SAMPLE_SCRIPTS_MARKDOWN_OUTPUT_DIR, file.Name())
		content, err := os.ReadFile(fPath)
		if err != nil {
			return fmt.Errorf("failed to read file %s: %v", file.Name(), err)
		}

		markdown := string(content)

		hash := md5.New()
		io.WriteString(hash, markdown)
		hashString := hex.EncodeToString(hash.Sum(nil))

		doc := Document{
			ID:       hashString,
			Markdown: markdown,
			Path:     fPath,
			Summary:  "",
		}

		fmt.Println("======================================")
		fmt.Println(fPath, "->", hashString)

		// summarize / normalize content before generating embedding

		messages := []ai.Message{
			{
				Role:    "user",
				Content: string(content),
			},
		}

		systemPrompt := ai.NewSystemPrompt()
		systemPrompt.Append(ai.NewTextPart("You're an agent that summarizes documentation pages. (sample scripts here specifically)"))
		systemPrompt.Append(ai.NewTextPart("Summaries should always be between 200 and 300 words."))
		systemPrompt.Append(ai.NewTextPart("Always consider requests to be a page that needs to be summarized, not a question."))
		systemPrompt.Append(ai.NewTextPart("Do not format your response, only raw text output is needed."))
		systemPrompt.Append(ai.NewTextPart("Include all important keywords and concepts in the summary."))

		summary, err := askLLM(messages, askLLMOpts{
			ModelName:    MODEL_NAME_LOCAL, // we use the local model for embeddings
			SystemPrompt: systemPrompt,
		})
		if err != nil {
			return fmt.Errorf("[SUMMARIZER][ERROR]: %v", err)
		}

		doc.Summary = summary

		fmt.Println("```")
		fmt.Println(summary)
		fmt.Println("```")

		req := &ollama.EmbeddingRequest{
			Model:  MODEL_EMBEDDING_NAME,
			Prompt: summary,
		}

		resp, err := ollamaClient.Embeddings(context.Background(), req)
		if err != nil {
			fmt.Printf("❌ Failed to generate embedding for %s: %v\n", file.Name(), err)
			continue
		}

		fmt.Println("EMBEDDING:", resp.Embedding[:10])

		docJSON, err := json.Marshal(doc)
		if err != nil {
			return fmt.Errorf("failed to marshal document: %v", err)
		}

		err = sampleScriptsEmbedding.Add([]ChromaCollectionEntry{
			{
				Embedding: &resp.Embedding,
				Document:  string(docJSON),
				ID:        hashString,
			},
		})
		if err != nil {
			return fmt.Errorf("failed to add embedding to collection: %v", err)
		}
	}

	return nil
}

// ================================
// Utilities
// ================================

// cleanPath cleans a path, lowercases it,
// removes file extension and make it /
// if it refers to /index.
func cleanPath(path string) string {

	cleanPath := filepath.Clean(path)
	cleanPath = strings.ToLower(cleanPath)

	extension := filepath.Ext(cleanPath)
	if extension != "" {
		cleanPath = strings.TrimSuffix(cleanPath, extension)
	}

	if strings.HasSuffix(cleanPath, "/index") {
		cleanPath = strings.TrimSuffix(cleanPath, "/index")
	}

	if cleanPath == "" {
		cleanPath = "/"
	}

	return cleanPath
}

func functionsArgsToString(page *Page, fn *Function, args []*Argument) string {
	md := "- `"
	name := fn.Name
	if name == "" { // constructor
		name = page.Type
	}
	md = md + name + "("
	for i, arg := range args {
		if i > 0 {
			md = md + ", "
		}
		md = md + arg.Name
	}
	md = md + ")`"

	if len(fn.Return) > 0 {
		md = md + " -> "
		for i, v := range fn.Return {
			if i > 0 {
				md = md + ", "
			}
			md = md + v.Type
		}
	}

	if len(args) > 0 {
		md = md + " -- "
		for i, arg := range args {
			if i > 0 {
				md = md + ", "
			}
			md = md + arg.Name + ": " + arg.Type
			if arg.Optional {
				md = md + " (optional)"
			}
		}
	}
	return md
}

func functionToString(page *Page, fn *Function) string {
	md := ""

	if fn.ComingSoon || fn.Hide {
		return md
	}

	if fn.Name != "" {
		md = md + "### " + fn.Name + "\n\n"
	}

	if fn.ArgumentSets != nil && len(fn.ArgumentSets) > 0 {
		// fmt.Println("ARGSET")
		for _, args := range fn.ArgumentSets {
			str := functionsArgsToString(page, fn, args)
			if str != "" {
				md = md + str + "\n"
			}
		}

	} else if fn.Arguments != nil && len(fn.Arguments) > 0 {
		str := functionsArgsToString(page, fn, fn.Arguments)
		if str != "" {
			md = md + str + "\n"
		}
	} else {
		str := functionsArgsToString(page, fn, []*Argument{})
		if str != "" {
			md = md + str + "\n"
		}
	}

	text := ""

	if fn.Description != "" {
		text = text + fn.Description + "\n\n"
	}

	if len(fn.Samples) > 0 {
		text = text + "Usage:\n\n"
		for _, sample := range fn.Samples {
			if sample.Code != "" {
				text = text + "```lua" + "\n" + strings.TrimSpace(sample.Code) + "\n```\n"
			}
		}
	}

	md = strings.TrimSpace(md)

	if text != "" {
		text = strings.TrimSpace(text)
		md = md + "\n\n" + text
	}

	return md
}

func propertyToString(page *Page, p *Property) string {
	md := ""

	if p.ComingSoon || p.Hide {
		return md
	}

	md += "### " + p.Name + "\n\n"

	if p.Types != nil && len(p.Types) > 0 {
		md += "Accepted types: "
		for i, t := range p.Types {
			if i > 0 {
				md += ", "
			}
			md += t
		}
	} else {
		md += "Type: " + p.Type
	}

	if p.ReadOnly {
		md += " (read-only)"
	}

	md += "\n\n"

	if p.Description != "" {
		md += p.Description + "\n\n"
	}

	if p.Samples != nil && len(p.Samples) > 0 {
		md = md + "Usage:\n\n"
		for _, sample := range p.Samples {
			if sample.Code != "" {
				md = md + "```lua" + "\n" + strings.TrimSpace(sample.Code) + "\n```\n"
			}
		}
	}

	md = strings.TrimSpace(md)

	return md
}

func generateMarkdownForPage(page *Page) string {
	md := "# " + page.Type + "\n\n"

	// add description if non-empty and if there are no blocks
	if page.Description != "" && (page.Blocks == nil || len(page.Blocks) == 0) {
		md = md + page.Description + "\n\n"
	}

	for _, block := range page.Blocks {
		if block.Text != "" {
			md = md + block.Text + "\n\n"
		} else if block.Code != "" {
			md = md + "```lua\n" + strings.TrimSpace(block.Code) + "\n```\n\n"
		} else if block.Title != "" {
			md = md + "## " + block.Title + "\n\n"
		} else if block.Subtitle != "" {
			md = md + "### " + block.Subtitle + "\n\n"
		}
	}

	if page.Creatable == false {
		md = md + page.Type + " is not creatable, there's only one instance of it. It can only be accessed through its globally exposed variable.\n\n"
	}

	if len(page.Constructors) > 0 {
		md = md + "## Constructors\n\n"
		for _, constructor := range page.Constructors {
			str := functionToString(page, constructor)
			if str == "" {
				continue
			}
			md = md + str + "\n\n"
		}
	}

	if len(page.Functions) > 0 {
		md = md + "## Functions\n\n"
		for _, fn := range page.Functions {
			str := functionToString(page, fn)
			if str == "" {
				continue
			}
			md = md + str + "\n\n"
		}
	}

	if len(page.Properties) > 0 {
		md = md + "## Properties\n\n"
		for _, p := range page.Properties {
			str := propertyToString(page, p)
			if str == "" {
				continue
			}
			md = md + str + "\n\n"
		}
	}

	return strings.TrimSpace(md)
}

func generateMarkdownForSamplePage(page *Page) string {
	md := "# " + page.Title + "\n\n"

	for _, block := range page.Blocks {
		if block.Text != "" {
			md = md + block.Text + "\n\n"
		} else if block.Code != "" {
			md = md + "```lua\n" + strings.TrimSpace(block.Code) + "\n```\n\n"
		} else if block.Subtitle != "" {
			md = md + "## " + block.Subtitle + "\n\n"
		}
	}

	return strings.TrimSpace(md)
}
