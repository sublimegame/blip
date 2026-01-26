package main

import (
	// "bytes"
	"errors"
	"fmt"
	"html/template"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"sync"
	"time"

	"amplitude"
	"utils"

	"github.com/gdevillele/frontparser"
	"github.com/gin-contrib/sessions"
	"github.com/gin-contrib/sessions/cookie"
	"github.com/gin-gonic/gin"
	"github.com/shurcooL/github_flavored_markdown"
	"go.mongodb.org/mongo-driver/mongo"
)

const (
	debugUsers               = 32
	SESSION_SECRET           = "ef967a9f96612e50bd9beb97d5190f79"
	NOTION_CACHE             = "/cache"
	NOTION_CACHE_FILES       = "/cache/files"
	NOTION_FILES_PUBLIC_PATH = "/files"
)

var (
	MAILJET_API_KEY    = os.Getenv("MAILJET_API_KEY")
	MAILJET_API_SECRET = os.Getenv("MAILJET_API_SECRET")
)

var (
	mongoClient *mongo.Client = nil

	notionPages *NotionPages = nil

	notionPagesMutex sync.RWMutex
)

func _trackEvent(deviceID string, eventType string, clientIP string, properties map[string]string) {
	event := &amplitude.Event{
		EventType:       eventType,
		Platform:        "Web",
		DeviceID:        deviceID,
		IP:              clientIP,
		EventProperties: properties,
	}
	err := amplitude.SendEvent(event)
	if err != nil {
		fmt.Println("amplitude error:", err.Error())
	}
}

func trackEvent(c *gin.Context, eventType string, properties map[string]string) {

	session := sessions.Default(c)

	userID := session.Get("userid")

	userIDStr := ""

	if userID == nil {
		uuid, err := utils.UUID()
		if err != nil {
			fmt.Println("❌ can't get UUID for Amplitude")
			return
		}

		userIDStr = uuid
		session.Set("userid", userIDStr)
		session.Save()

	} else {

		ok := false
		userIDStr, ok = userID.(string)
		if ok == false {
			uuid, err := utils.UUID()
			if err != nil {
				fmt.Println("❌ can't get UUID for Amplitude")
				return
			}

			userIDStr = uuid
			session.Set("userid", userIDStr)
			session.Save()
		}
	}

	go _trackEvent(userIDStr, eventType, c.ClientIP(), properties)
}

func main() {

	notionPages = syncPages()

	quit := make(chan struct{})

	go func() {
		for {
			time.Sleep(1 * time.Minute)

			select {
			case <-quit:
				return
			default:
				updatedPages := syncPages()

				notionPagesMutex.Lock()
				notionPages = updatedPages
				notionPagesMutex.Unlock()
			}
		}
	}()

	var err error

	mongoClient, err = connect()
	if err != nil {
		fmt.Println("😕 can't connect to database")
	}

	fmt.Println("👍 connected to database")

	fmt.Println("✨ starting particubes web server...")

	r := gin.Default()

	store := cookie.NewStore([]byte(SESSION_SECRET))
	r.Use(sessions.Sessions("session", store))

	// redirections
	setRedirection(r, "/get", "https://itunes.apple.com/app/id1299143207?mt=8")
	setRedirection(r, "/download-mac-alpha", "https://download.particubes.com/Particubes-0.0.5.dmg")
	setRedirection(r, "/discord", "https://discord.gg/NbpdAkv")
	setRedirection(r, "/discord-fr", "https://discord.gg/xVSqdJu")
	setRedirection(r, "/discord-mc-regles", "https://discord.gg/WAvY7Kn")
	setRedirection(r, "/teaser", "https://youtu.be/E3ioQ7L2Uew")

	// templates
	addTemplatesDir(r, "/assets/templates")
	// serve static files
	r.Static("/style", "/assets/style")
	r.Static("/img", "/assets/img")
	r.Static("/js", "/assets/js")
	r.Static("/video", "/assets/video")
	r.Static("/private", "/private")
	r.Static(NOTION_FILES_PUBLIC_PATH, NOTION_CACHE_FILES) // files coming from Notion

	r.GET("/", index)

	r.GET("/f/:origin", indexFrom)

	r.GET("/confirm/:hash", getConfirm)
	r.GET("/privacy-policy", privacy)
	// r.GET("/faq", faq)
	// r.GET("/contact", contact)
	// r.GET("/press", press)

	r.NoRoute(noRoute)
	r.Run(":80") // listen and serve on 0.0.0.0:80 (for windows "localhost:80")

	close(quit)
}

// GET /
func index(c *gin.Context) {

	trackEvent(c, "page_index", nil)

	// count, err := getUserCount(mongoClient)
	// if err != nil {
	// 	renderError(c, "Oops", "Internal server error.", http.StatusInternalServerError)
	// 	return
	// }

	htmlParams := gin.H{
		"headTitle": "Particubes",
		// "nextTester": strconv.FormatInt(count+1-debugUsers, 10),
	}

	notionPagesMutex.Lock()
	pages := notionPages
	notionPagesMutex.Unlock()

	if page, ok := pages.Pages["/"]; ok {
		fmt.Println("BLOCKS:", len(page.Blocks))

		htmlParams["introTitle"] = page.getHeaderBlockText(1)
		htmlParams["introBlocks"] = page.getBlocksAfterHeader(1)

		htmlParams["exploreTitle"] = page.getHeaderBlockText(2)
		htmlParams["exploreBlocks"] = page.getBlocksAfterHeader(2)

		htmlParams["buildTitle"] = page.getHeaderBlockText(3)
		htmlParams["buildBlocks"] = page.getBlocksAfterHeader(3)

		htmlParams["codeTitle"] = page.getHeaderBlockText(4)
		htmlParams["codeBlocks"] = page.getBlocksAfterHeader(4)
	}

	c.HTML(http.StatusOK, "index.tmpl", htmlParams)
}

func indexFrom(c *gin.Context) {
	origin := c.Param("origin")
	trackEvent(c, "page_index_from", map[string]string{"origin": origin})
	c.Redirect(http.StatusFound, "/") // HTTP 302
}

// GET /privacy-policy
// Renders the privacy-policy page
func privacy(c *gin.Context) {
	trackEvent(c, "page_privacy", nil)
	renderMarkdown(c, "/assets/pages/en/privacy-policy.md")
}

// GET /confirm
func getConfirm(c *gin.Context) {

	trackEvent(c, "page_confirm", nil)

	// get confirm hash value from path
	hash := c.Param("hash")
	if len(hash) == 0 {
		renderError(c, "Oops", "Invalid request.", http.StatusBadRequest)
		return
	}

	// confirm e-mail address in DB
	ok, err := confirmEmailAddress(mongoClient, hash)
	if err != nil {
		renderError(c, "Oops", "Internal server error.", http.StatusInternalServerError)
		return
	}

	// ok == true
	// Means the email has been confirmed for the concerned account
	//
	// ok == false
	// Means not account was found for this confirmation hash

	if ok == false {
		renderError(c, "Oops", "Internal server error.", http.StatusBadRequest)
	} else {

		title := "Email confirmation"

		htmlParams := gin.H{
			"headTitle": "Particubes - " + title,
			"header":    title,
		}

		c.HTML(http.StatusOK, "emailAddressConfirmation.tmpl", htmlParams)
	}
}

/// 404
func noRoute(c *gin.Context) {

	// see if we have a page in Notion for this path:
	path := c.Request.URL.Path

	notionPagesMutex.Lock()
	pages := notionPages
	notionPagesMutex.Unlock()

	if page, ok := pages.Pages[path]; ok {
		// renderError(c, "Oops", "work in progress...", http.StatusInternalServerError)
		renderNotionPage(c, page)
		return
	}

	trackEvent(c, "page_404", map[string]string{"path": path})

	title := "Oh no!"

	htmlParams := gin.H{
		"headTitle": "Particubes - " + title,
		"header":    title,
		"error":     "Page not found...",
	}
	c.HTML(http.StatusNotFound, "error.tmpl", htmlParams)
}

func renderNotionPage(c *gin.Context, page *NotionPage) {

	htmlParams := gin.H{
		"language":        "en",
		"headTitle":       "Particubes - " + page.Title,
		"header":          page.Title,
		"title":           page.Title,
		"titleIcon":       page.Icon,
		"metaKeywords":    make([]string, 0),
		"metaDescription": "",
		"page":            page,
		"cover":           page.Cover,
	}

	c.HTML(http.StatusOK, "notionPage.tmpl", htmlParams)
}

func renderMarkdown(c *gin.Context, mdPath string) {

	mdFileBytes, err := ioutil.ReadFile(mdPath)
	if err != nil {
		fmt.Println("❌", err.Error())
		renderError(c, "Oops", "Internal server error.", http.StatusInternalServerError)
		return
	}

	pageLanguage := "en"
	pageTitle := ""
	pageKeywords := ""
	pageDescription := ""
	pageMdContent := mdFileBytes
	pageTemplate := "page.tmpl" // default
	pageFrontmatter := make(map[string]interface{})
	collapseH3 := false
	titleIcon := "glitter"

	// frontmatter parsing
	// check if file has a frontmatter header
	if frontparser.HasFrontmatterHeader(mdFileBytes) {
		fm, md, err := frontparser.ParseFrontmatterAndContent(mdFileBytes)
		if err != nil {
			fmt.Println("❌", err.Error())
			renderError(c, "Oops", "Internal server error.", http.StatusInternalServerError)
			return
		}
		pageFrontmatter = fm
		pageMdContent = md
		// find title in frontmatter
		if titleIface, ok := pageFrontmatter["title"]; ok {
			titleStr, err := toString(titleIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter title value is not a string.")
			} else {
				pageTitle = titleStr
			}
		}
		// find keywords in frontmatter
		if keywordsIface, ok := pageFrontmatter["keywords"]; ok {
			keywordsStr, err := toString(keywordsIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter keywords value is not a string")
			} else {
				pageKeywords = keywordsStr
			}
		}
		// find description in frontmatter
		if descriptionIface, ok := pageFrontmatter["description"]; ok {
			descriptionStr, err := toString(descriptionIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter description value is not a string")
			} else {
				pageDescription = descriptionStr
			}
		}
		// find template in frontmatter
		if templateIface, ok := pageFrontmatter["template"]; ok {
			templateStr, err := toString(templateIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter template value is not a string")
			} else {
				pageTemplate = templateStr
			}
		}

		if titleIconIface, ok := pageFrontmatter["title-icon"]; ok {
			titleIconStr, err := toString(titleIconIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter title-icon value is not a string")
			} else {
				titleIcon = titleIconStr
			}
		}

		if collapseH3IFace, ok := pageFrontmatter["collapse-h3"]; ok {
			collapseH3Bool, err := toBool(collapseH3IFace)
			if err != nil {
				fmt.Println("ERROR: frontmatter collapse-h3 value is not a boolean")
			} else {
				fmt.Println("collapseH3Bool:", collapseH3Bool)
				collapseH3 = collapseH3Bool
			}
		}
	}

	html := github_flavored_markdown.Markdown(pageMdContent)

	if collapseH3 {
		// INJECT COLLAPSIBLE CLASSES:
		reCollapsibles := regexp.MustCompile(`</h3>(([^<]|<[^h])*)`)
		html = reCollapsibles.ReplaceAll(html, []byte("</h3><div class=\"collapsible hidden\">${1}</div><noscript>${1}</noscript>"))
		reToggleLinks := regexp.MustCompile(`<h3>(([^<]|<[^/]|</[^a]|</a[^>])*)</a>`)
		html = reToggleLinks.ReplaceAll(html, []byte("<h3>"))
		reToggleLinks2 := regexp.MustCompile(`<h3>(([^<]|<[^/])*)`)
		html = reToggleLinks2.ReplaceAll(html, []byte("<h3><a class=\"toggle-collapsible\" href=\"\">${1}</a>"))
	}

	htmlParams := gin.H{
		"language":        pageLanguage,
		"headTitle":       "Particubes - " + pageTitle,
		"title":           pageTitle,
		"titleIcon":       titleIcon,
		"metaKeywords":    pageKeywords,
		"metaDescription": pageDescription,
		"content":         template.HTML(html),
	}

	c.HTML(http.StatusOK, pageTemplate, htmlParams)
}

// --------------------------------------------------
//
// UTILITY FUNCTION
//
// --------------------------------------------------

func setRedirection(r *gin.Engine, path string, destination string) {
	r.GET(path, func(c *gin.Context) {
		c.Redirect(http.StatusFound, destination) // HTTP 302
	})
}

func addTemplatesDir(r *gin.Engine, templateDir string) {
	tmplFiles := make([]string, 0)
	files, err := ioutil.ReadDir(templateDir)
	if err != nil {
		return
	}
	for _, file := range files {
		tmplFiles = append(tmplFiles, filepath.Join(templateDir, file.Name()))
	}
	r.LoadHTMLFiles(tmplFiles...)
}

func renderError(c *gin.Context, title string, msg string, statusCode int) {

	trackEvent(c, "page_error", nil)

	htmlParams := gin.H{
		"headTitle": "Particubes - " + title,
		"header":    title,
		"error":     msg,
	}

	c.HTML(statusCode, "error.tmpl", htmlParams)
}

func renderErrorWithForm(c *gin.Context, title string, msg string, email string, statusCode int) {

	trackEvent(c, "page_form_error", nil)

	htmlParams := gin.H{
		"headTitle": "Particubes - " + title,
		"header":    title,
		"error":     msg,
		"email":     email,
	}

	count, err := getUserCount(mongoClient)
	if err == nil {
		htmlParams["nextTester"] = strconv.FormatInt(count+1-debugUsers, 10)
	}

	// render page
	c.HTML(statusCode, "error.tmpl", htmlParams)
}

func toString(i interface{}) (string, error) {
	if str, ok := i.(string); ok {
		return str, nil
	}
	return "", errors.New("interface is not a string")
}

func toBool(i interface{}) (bool, error) {
	if b, ok := i.(bool); ok {
		return b, nil
	}
	return false, errors.New("interface is not a bool")
}
