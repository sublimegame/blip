package main

import (
	// "encoding/json"
	"fmt"
	"net/url"
	"path/filepath"
	"strings"

	"github.com/kjk/notionapi"
)

const (
	BlockText    = "text"
	BlockImage   = "image"
	BlockVideo   = "video"
	BlockHeader  = "header"
	BlockCode    = "code"
	BlockToggle  = "toggle"
	BlockTodo    = "to_do"
	BlockColumns = "column_list"
	BlockColumn  = "column"
	BulletedList = "bulleted_list"
	BlockDivider = "divider"
)

type NotionPages struct {
	// pages indexed by path
	Pages map[string]*NotionPage `json:"pages,omitempty"`
}

type NotionPage struct {
	ID   string `json:"id,omitempty"`
	Path string `json:"path,omitempty"`

	Title string `json:"title,omitempty"`

	Cover string `json:"cover,omitempty"`

	Icon string `json:"icon,omitempty"`

	Blocks []*NotionBlock `json:"blocks,omitempty"`
}

type NotionBlock struct {
	Type string `json:"type,omitempty"`

	// Texts (including headers)
	// 0 when not a header
	HeaderLevel int         `json:"header,omitempty"`
	TextSpans   []*TextSpan `json:"text,omitempty"`

	// Images
	Image *Image `json:"image,omitempty"`

	Video *Video `json:"video,omitempty"`

	// sub blocks
	// represent columns when type is BlockColumns for example
	Blocks []*NotionBlock `json:"blocks,omitempty"`
}

type TextSpan struct {
	Text   string `json:"text,omitempty"`
	Link   string `json:"link,omitempty"`
	Bold   bool   `json:"b,omitempty"`
	Italic bool   `json:"i,omitempty"`
}

type Image struct {
	Path string `json:"path,omitempty"`
	Link string `json:"link,omitempty"`
	Alt  string `json:"alt,omitempty"`
}

type Video struct {
	URL      string `json:"url,omitempty"`
	Provider string `json:"provider,omitempty"`
}

func (p *NotionPage) getHeaderBlockText(pos int) string {

	i := 0

	for _, block := range p.Blocks {

		if block.Type == BlockHeader {
			i = i + 1
			if i == pos {

				joined := ""

				for _, span := range block.TextSpans {
					joined += span.Text
				}

				return joined
			}
		}
	}

	return ""
}

func (p *NotionPage) getBlocksAfterHeader(pos int) []*NotionBlock {

	i := 0

	blocks := make([]*NotionBlock, 0)

	reachedHeader := false

	for _, block := range p.Blocks {

		if reachedHeader == false {

			if block.Type == BlockHeader {
				i = i + 1
				if i == pos {
					reachedHeader = true
				}
			}
		} else {
			if block.Type == BlockHeader {
				// return when finding next header
				return blocks
			}

			blocks = append(blocks, block)
		}
	}

	return blocks
}

func syncPages() *NotionPages {

	fmt.Println("sync Notion pages...")

	pages := &NotionPages{Pages: make(map[string]*NotionPage)}

	err := syncPage("b9a43bf0ce024d468622f36418110e87", "/", "", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	err = syncPage("f92baf736e004adb82364fc6a707645a", "/faq", "question", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	err = syncPage("5a850cf089374af1a437578dd9102fb4", "/press", "news", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	err = syncPage("e04a1f825f914d5eb9a67e2e07e9503d", "/beginners-guide", "", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	err = syncPage("a6422284c6974ecebb4336170e87aebf", "/download", "download", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	err = syncPage("ba903f0f2bd64f87a39a3a1c8b7cc8b8", "/contact", "heart", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	err = syncPage("88b4dd2e7845435daf11666d8e7374fc", "/help/windows-file-permissions", "question", pages)
	if err != nil {
		fmt.Println("❌ can't download page:", err.Error())
	}

	// DEBUG
	// b, err := json.Marshal(pages)
	// if err != nil {
	// 	fmt.Println("❌ can't json encode pages:", err.Error())
	// }
	// fmt.Println(string(b))

	return pages
}

func syncPage(notionPageID string, path string, titleIcon string, pages *NotionPages) error {

	// When columns isn't nil, it means we're parsing columns
	// not adding new Blocks at the top level
	// NOTE: we don't support columns within columns...
	var columns *NotionBlock = nil
	var currentColumn *NotionBlock = nil
	nbColumnsToParse := 0
	nbColumnBlocksToParse := 0

	var currentToggle *NotionBlock = nil
	nbToggleBlocksToParse := 0

	p := &NotionPage{
		ID:     notionPageID,
		Path:   path,
		Title:  "",
		Cover:  "",
		Icon:   titleIcon,
		Blocks: make([]*NotionBlock, 0),
	}

	client := &notionapi.Client{}
	cachingClient, err := notionapi.NewCachingClient("/cache", client)

	if err != nil {
		return err
	}

	page, err := cachingClient.DownloadPage(notionPageID)
	if err != nil {
		return err
	}

	page.ForEachBlock(func(block *notionapi.Block) {

		if currentColumn != nil && nbColumnBlocksToParse == 0 {
			currentColumn = nil
			if columns != nil && nbColumnsToParse == 0 {
				columns = nil
			}
		}

		if currentToggle != nil && nbToggleBlocksToParse == 0 {
			currentToggle = nil
		}

		switch block.Type {
		case notionapi.BlockPage:

			p.Title = block.Title

			page := block.FormatPage()

			if page != nil {

				resp, err := cachingClient.DownloadFile(page.PageCover, block)
				if err != nil {
					fmt.Println("❌ can't download file:", page.PageCover)
					return
				}

				p.Cover = filepath.Join(NOTION_FILES_PUBLIC_PATH, filepath.Base(resp.CacheFilePath))
			}

		case notionapi.BlockHeader, notionapi.BlockSubHeader, notionapi.BlockSubSubHeader, notionapi.BlockText:

			b := &NotionBlock{
				Type:      BlockText,
				TextSpans: make([]*TextSpan, 0),
			}

			if block.Type == notionapi.BlockHeader {
				b.Type = BlockHeader
				b.HeaderLevel = 1
			} else if block.Type == notionapi.BlockSubHeader {
				b.Type = BlockHeader
				b.HeaderLevel = 2
			} else if block.Type == notionapi.BlockSubSubHeader {
				b.Type = BlockHeader
				b.HeaderLevel = 3
			}

			for _, span := range block.InlineContent {

				s := &TextSpan{
					Text:   span.Text,
					Bold:   false,
					Italic: false,
					Link:   "",
				}

				for _, attr := range span.Attrs {

					switch attr[0] {
					case notionapi.AttrBold:
						s.Bold = true
					case notionapi.AttrItalic:
						s.Italic = true
					case notionapi.AttrLink:
						s.Link = attr[1]
					default:
						return
					}
				}

				b.TextSpans = append(b.TextSpans, s)
			}

			// add to page or current container
			if currentColumn != nil {
				currentColumn.Blocks = append(currentColumn.Blocks, b)
				nbColumnBlocksToParse = nbColumnBlocksToParse - 1
			} else if currentToggle != nil {
				currentToggle.Blocks = append(currentToggle.Blocks, b)
				nbToggleBlocksToParse = nbToggleBlocksToParse - 1
			} else {
				p.Blocks = append(p.Blocks, b)
			}

		case notionapi.BlockToggle:

			if columns != nil {
				fmt.Println("❌ toggle within columns not supported")
				return
			}

			if currentColumn != nil {
				fmt.Println("❌ toggle within columns not supported")
				return
			}

			b := &NotionBlock{
				Type:      BlockToggle,
				TextSpans: make([]*TextSpan, 0),
				Blocks:    make([]*NotionBlock, 0),
			}

			for _, span := range block.InlineContent {

				s := &TextSpan{
					Text:   span.Text,
					Bold:   false,
					Italic: false,
					Link:   "",
				}

				for _, attr := range span.Attrs {

					switch attr[0] {
					case notionapi.AttrBold:
						s.Bold = true
					case notionapi.AttrItalic:
						s.Italic = true
					case notionapi.AttrLink:
						s.Link = attr[1]
					default:
						return
					}
				}

				b.TextSpans = append(b.TextSpans, s)
			}

			currentToggle = b
			nbToggleBlocksToParse = len(block.Content)

			p.Blocks = append(p.Blocks, b)

		case notionapi.BlockDivider:

			b := &NotionBlock{
				Type: BlockDivider,
			}

			// add to page or current container
			if currentColumn != nil {
				currentColumn.Blocks = append(currentColumn.Blocks, b)
				nbColumnBlocksToParse = nbColumnBlocksToParse - 1
			} else if currentToggle != nil {
				currentToggle.Blocks = append(currentToggle.Blocks, b)
				nbToggleBlocksToParse = nbToggleBlocksToParse - 1
			} else {
				p.Blocks = append(p.Blocks, b)
			}

		case notionapi.BlockImage:

			b := &NotionBlock{
				Type: BlockImage,
				Image: &Image{
					Path: "",
					Link: "",
				},
			}

			resp, err := cachingClient.DownloadFile(block.Source, block)
			if err != nil {
				fmt.Println("❌ can't download file:", block.Source)
				return
			}

			b.Image.Path = filepath.Join(NOTION_FILES_PUBLIC_PATH, filepath.Base(resp.CacheFilePath))

			spans := block.GetCaption()
			joined := ""

			for _, span := range spans {
				joined += span.Text
			}

			b.Image.Alt = joined

			if isValidUrl(joined) {
				b.Image.Link = joined
			}

			// add to page or current container
			if currentColumn != nil {
				currentColumn.Blocks = append(currentColumn.Blocks, b)
				nbColumnBlocksToParse = nbColumnBlocksToParse - 1
			} else if currentToggle != nil {
				currentToggle.Blocks = append(currentToggle.Blocks, b)
				nbToggleBlocksToParse = nbToggleBlocksToParse - 1
			} else {
				p.Blocks = append(p.Blocks, b)
			}

		case notionapi.BlockVideo:

			b := &NotionBlock{
				Type: BlockVideo,
				Video: &Video{
					URL:      "",
					Provider: "",
				},
			}

			b.Video.URL = block.Source

			if strings.Contains(b.Video.URL, "youtube") {
				b.Video.Provider = "youtube"
			}

			b.Video.URL = strings.ReplaceAll(b.Video.URL, "watch?v=", "embed/")

			// add to page or current container
			if currentColumn != nil {
				currentColumn.Blocks = append(currentColumn.Blocks, b)
				nbColumnBlocksToParse = nbColumnBlocksToParse - 1
			} else if currentToggle != nil {
				currentToggle.Blocks = append(currentToggle.Blocks, b)
				nbToggleBlocksToParse = nbToggleBlocksToParse - 1
			} else {
				p.Blocks = append(p.Blocks, b)
			}

		case notionapi.BlockBulletedList:

			b := &NotionBlock{
				Type: BulletedList,
				// Blocks:    make([]*NotionBlock, 0),
				TextSpans: make([]*TextSpan, 0),
			}

			for _, span := range block.InlineContent {

				s := &TextSpan{
					Text:   span.Text,
					Bold:   false,
					Italic: false,
					Link:   "",
				}

				for _, attr := range span.Attrs {

					switch attr[0] {
					case notionapi.AttrBold:
						s.Bold = true
					case notionapi.AttrItalic:
						s.Italic = true
					case notionapi.AttrLink:
						s.Link = attr[1]
					default:
						return
					}
				}

				b.TextSpans = append(b.TextSpans, s)
			}

			// add to page or current container
			if currentColumn != nil {
				currentColumn.Blocks = append(currentColumn.Blocks, b)
				nbColumnBlocksToParse = nbColumnBlocksToParse - 1
			} else if currentToggle != nil {
				currentToggle.Blocks = append(currentToggle.Blocks, b)
				nbToggleBlocksToParse = nbToggleBlocksToParse - 1
			} else {
				p.Blocks = append(p.Blocks, b)
			}

		case notionapi.BlockColumnList:

			if currentToggle != nil {
				fmt.Println("❌ columns within toggle not supported")
				return
			}

			b := &NotionBlock{
				Type:   BlockColumns,
				Blocks: make([]*NotionBlock, 0),
			}

			columns = b
			currentColumn = nil
			nbColumnsToParse = len(block.Content)

			p.Blocks = append(p.Blocks, b)

		case notionapi.BlockColumn:

			if columns == nil {
				fmt.Println("❌ column block not in columns")
				return
			}

			b := &NotionBlock{
				Type:   BlockColumn,
				Blocks: make([]*NotionBlock, 0),
			}

			currentColumn = b
			nbColumnsToParse = nbColumnsToParse - 1
			nbColumnBlocksToParse = len(block.Content)

			columns.Blocks = append(columns.Blocks, b)

		default:
			fmt.Println("⚠️ block type not supported", block.Type)
			return
		}

	})

	pages.Pages[path] = p

	return nil
}

func isValidUrl(str string) bool {
	_, err := url.ParseRequestURI(str)
	if err != nil {
		return false
	}

	u, err := url.Parse(str)
	if err != nil || u.Scheme == "" || u.Host == "" {
		return false
	}

	return true
}

// func afterDownload(info *notionapi.DownloadInfo) error {
// 	fmt.Println("INFO", info)
// 	return nil
// }
