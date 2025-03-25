package main

import (
	"container/list"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"strings"
	"text/template"

	"github.com/gosimple/slug"
	yaml "gopkg.in/yaml.v2"
)

const (
	contentDirectory = "/www"
	templateDir      = "/www/templates"
	templateFile     = "page.tmpl"
	templateFileV2   = "pageV2.tmpl"
)

var (
	debug bool = true

	pages   map[string]*Page
	pagesV2 map[string]*Module

	pageTemplate   *template.Template
	pageTemplateV2 *template.Template

	staticFileDirectories = []string{"js", "style", "audio", "images", "media"}
	// key: the type, value: the page where it is described
	typeRoutes map[string]string
)

// Redirect middleware docs.cu.bzh -> docs.blip.game
func redirectMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		hostParts := strings.Split(r.Host, ":") // port may or may not be present
		if len(hostParts) > 0 && hostParts[0] == "docs.cu.bzh" {
			target := "https://docs.blip.game" + r.URL.Path
			if r.URL.RawQuery != "" {
				target += "?" + r.URL.RawQuery
			}
			http.Redirect(w, r, target, http.StatusMovedPermanently)
			return
		}
		next.ServeHTTP(w, r)
	})
}

func main() {

	nbArgs := len(os.Args)

	if nbArgs > 1 {
		command := os.Args[1]
		if command == "test" {

			err := parseContent()
			if err != nil {
				fmt.Println("ERR:", err.Error())
				os.Exit(1)
			}

			fmt.Println("OK")
			return
		}
	}

	// --------------------------------------------------
	// retrieve environment variables' values
	// --------------------------------------------------
	if os.Getenv("RELEASE") == "1" {
		debug = false
	}
	fmt.Println("debug:", debug)

	err := parseContent()
	if err != nil {
		log.Fatalf("%v", err)
	}

	// --------------------------------------------------
	// Setup HTTP server
	// --------------------------------------------------

	// Create the mux
	mux := http.NewServeMux()

	// Define the routes
	for _, staticDir := range staticFileDirectories {
		mux.Handle(
			"/"+staticDir+"/",
			http.StripPrefix("/"+staticDir+"/", http.FileServer(http.Dir(filepath.Join(contentDirectory, staticDir)))),
		)
	}
	mux.HandleFunc("/", httpHandler)

	// Wrap the entire mux with the middleware
	wrappedMux := redirectMiddleware(mux)

	// --------------------------------------------------
	// Start HTTP server
	// --------------------------------------------------

	fmt.Println("✨ Blip docs server is running...")
	log.Fatal(http.ListenAndServe(":80", wrappedMux))
}

func httpHandler(w http.ResponseWriter, r *http.Request) {

	if debug {
		parseContent()
	}

	path := cleanPath(r.URL.Path)

	page, ok := pages[path]

	if ok {
		if r.URL.Path != path {
			fmt.Println(r.URL.Path, "!=", path)
			http.Redirect(w, r, path, http.StatusMovedPermanently)
			return
		}

		if page != nil {
			_ = replyPage(w, page)
			return
		}
	}

	module, ok := pagesV2[path]

	if ok {
		if r.URL.Path != path {
			fmt.Println(r.URL.Path, "!=", path)
			http.Redirect(w, r, path, http.StatusMovedPermanently)
			return
		}

		if module != nil {
			_ = replyModule(w, module)
			return
		}
	}

	if path != "/" {
		// not found, redirect to /
		fmt.Println("not found:", path)

		if page404, ok := pages["/404"]; ok {
			w.WriteHeader(http.StatusNotFound)
			_ = replyPage(w, page404)
			return
		}

		http.Redirect(w, r, "/", http.StatusSeeOther)
		return
	}

	replyText(w, "hello world")
}

func replyText(w http.ResponseWriter, text string) {
	fmt.Fprintln(w, text)
}

func replyPage(w http.ResponseWriter, page *Page) error {
	err := pageTemplate.Execute(w, page)
	if err != nil {
		fmt.Println("🔥 error:", err.Error())
	}
	return err
}

func replyModule(w http.ResponseWriter, module *Module) error {
	err := pageTemplateV2.Execute(w, module)
	if err != nil {
		fmt.Println("🔥 error:", err.Error())
	}
	return err
}

func GetTitle(page *Page) string {
	return page.GetTitle()
}

func GetTitleV2(module *Module) string {
	return module.Name
}

func IsNotCreatableObject(page *Page) bool {
	return page.IsNotCreatableObject()
}

// GetTypeLink returns an non empty string if the type
// is defined at some route
func GetTypeRoute(typeName string) string {
	if route, ok := typeRoutes[typeName]; ok {
		return route
	}
	return "#type-" + slug.Make(typeName) // local type
}

// parseContent is only done once at startup in RELEASE mode.
// Called for each request in DEBUG to consider potential changes.
func parseContent() error {

	var err error

	pages = make(map[string]*Page)
	pagesV2 = make(map[string]*Module)

	typeRoutes = make(map[string]string)

	if !directoryExists(contentDirectory) {
		return errors.New("content directory is missing")
	}

	headTmplPath := filepath.Join(templateDir, "head.tmpl")
	footerTmplPath := filepath.Join(templateDir, "footer.tmpl")
	headerTmplPath := filepath.Join(templateDir, "header.tmpl")
	menuTmplPath := filepath.Join(templateDir, "menu.tmpl")
	sidemenuTmplPath := filepath.Join(templateDir, "sidemenu.tmpl")
	contentblocksTmplPath := filepath.Join(templateDir, "contentblocks.tmpl")
	typesTmplPath := filepath.Join(templateDir, "types.tmpl")

	templateFilePath := filepath.Join(templateDir, templateFile)

	pageTemplate = template.New("page.tmpl").Funcs(template.FuncMap{
		"Join":                  strings.Join,
		"GetTitle":              GetTitle,
		"GetAnchorLink":         GetAnchorLink,
		"SampleHasCodeAndMedia": SampleHasCodeAndMedia,
		"IsNotCreatableObject":  IsNotCreatableObject,
		"GetTypeRoute":          GetTypeRoute,
	})

	pageTemplate, err = pageTemplate.ParseFiles(headTmplPath, footerTmplPath, headerTmplPath, menuTmplPath, sidemenuTmplPath, contentblocksTmplPath, typesTmplPath, templateFilePath)
	if err != nil {
		fmt.Println("🔥 error:", err.Error())
		return err
	}

	templateFilePathV2 := filepath.Join(templateDir, templateFileV2)

	pageTemplateV2 = template.New("pageV2.tmpl").Funcs(template.FuncMap{
		"Join":                  strings.Join,
		"GetTitle":              GetTitleV2,
		"GetAnchorLink":         GetAnchorLink,
		"SampleHasCodeAndMedia": SampleHasCodeAndMedia,
		"IsNotCreatableObject":  IsNotCreatableObject,
		"GetTypeRoute":          GetTypeRoute,
	})

	pageTemplateV2, err = pageTemplateV2.ParseFiles(headTmplPath, footerTmplPath, headerTmplPath, menuTmplPath, sidemenuTmplPath, contentblocksTmplPath, typesTmplPath, templateFilePathV2)
	if err != nil {
		fmt.Println("🔥 error:", err.Error())
		return err
	}

	err = filepath.Walk(contentDirectory, func(walkPath string, walkInfo os.FileInfo, walkErr error) (err error) {
		if walkErr != nil {
			return walkErr
		}

		if strings.HasSuffix(walkPath, ".yml") { // YML FILE

			// check if path points to a regular file
			exists := regularFileExists(walkPath)
			if exists {

				var page Page

				file, err := os.Open(walkPath)
				if err != nil {
					return err
				}

				// example: from /www/index.yml to /index.yml
				trimmedPath := strings.TrimPrefix(walkPath, contentDirectory)

				page.ResourcePath = trimmedPath

				cleanPath := cleanPath(trimmedPath)

				err = yaml.NewDecoder(file).Decode(&page)

				if err != nil {
					return fmt.Errorf("%s %v", trimmedPath, err)
				}

				pages[cleanPath] = &page
			}

		} else if strings.HasSuffix(walkPath, ".json") { // JSON FILE

			// check if path points to a regular file
			exists := regularFileExists(walkPath)
			if exists {

				var module Module

				file, err := os.Open(walkPath)
				if err != nil {
					return err
				}

				// example: from /www/index.json to /index.json
				trimmedPath := strings.TrimPrefix(walkPath, contentDirectory)

				module.ResourcePath = trimmedPath

				cleanPath := cleanPath(trimmedPath)

				err = json.NewDecoder(file).Decode(&module)

				if err != nil {
					return fmt.Errorf("%s %v", trimmedPath, err)
				}

				module.Name = path.Base(cleanPath)

				// fmt.Println("from json:", cleanPath)
				pagesV2[cleanPath] = &module
			}
		}

		return nil
	})

	if err != nil {
		return err
	}

	for route, page := range pages {
		if page.Type != "" {
			typeRoutes[page.Type] = route
		}
	}

	queue := list.New()
	for _, page := range pages {
		if page.Type != "" && page.Extends != "" {
			page.ExtentionBaseSet = false
			queue.PushBack(page)
		}
	}

	for queue.Len() > 0 {
		e := queue.Front()
		page := e.Value.(*Page) // First element
		queue.Remove(e)         // Dequeue

		if extendedTypePageRoute, ok := typeRoutes[page.Extends]; ok {
			if extendedTypePage, ok := pages[extendedTypePageRoute]; ok {
				if extendedTypePage.ReadyToBeSetAsBase() {
					page.SetExtentionBase(extendedTypePage)
					page.ExtentionBaseSet = true
				} else {
					// base is itself an extension and should be
					// taken care of first.
					queue.PushBack(page)
				}
			}
		}
	}

	for _, page := range pages {
		page.Sanitize()
	}

	for _, module := range pagesV2 {
		module.Sanitize()
	}

	fmt.Println("content parsed!")
	// fmt.Printf("%4v\n", pages)
	// fmt.Println("PAGES:")
	// for k, _ := range pages {
	// 	fmt.Println(k)
	// }
	// fmt.Println("TYPES:")
	// for t, r := range typeRoutes {
	// 	fmt.Println(t, "->", r)
	// }

	return nil
}

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
