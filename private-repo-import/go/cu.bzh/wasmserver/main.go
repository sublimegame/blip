package main

import (
	"bytes"
	"compress/gzip"
	"crypto/md5"
	"encoding/json"
	"fmt"
	"io"
	"mime"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"

	"cu.bzh/cors"
)

const (
	EDITION_DIR           = "editions/"
	LATEST_DIR            = EDITION_DIR + "latest/"
	LISTEN_ADDR           = ":80"
	LISTEN_ADDR_TLS       = ":443"
	ENV_VAR_SECURE        = "SECURE"
	ENV_VAR_TLS_CERT_PATH = "TLS_CERT_PATH"
	ENV_VAR_TLS_KEY_PATH  = "TLS_KEY_PATH"
)

var (
	useSecureMode    bool            = false // default to HTTP
	useInMemoryCache bool            = false
	tlsCertPath      string          = ""
	tlsKeyPath       string          = ""
	files            map[string]File = make(map[string]File)
	mtx                              = &sync.Mutex{}
)

type File struct {
	Bytes                []byte
	Etag                 string
	ContentLength        string
	ContentType          string
	GZipped              []byte
	GZippedContentLength string
	Path                 string // original file path
}

func main() {
	var err error

	// --------------------------------------------------
	// Read environment variables
	// --------------------------------------------------
	{
		envVarSecure := os.Getenv(ENV_VAR_SECURE)
		if envVarSecure == "1" || envVarSecure == "true" {
			useSecureMode = true
		}
		if useSecureMode {
			tlsCertPath = os.Getenv(ENV_VAR_TLS_CERT_PATH)
			tlsKeyPath = os.Getenv(ENV_VAR_TLS_KEY_PATH)
			if tlsCertPath == "" || tlsKeyPath == "" {
				fmt.Println("🔥🔥🔥 TLS_CERT_PATH and TLS_KEY_PATH must be set when SECURE is true")
				os.Exit(1)
			}
		}
	}

	// --------------------------------------------------
	// HTTPS router
	// --------------------------------------------------
	router := chi.NewRouter()
	router.Use(middleware.Logger)
	router.Use(middleware.Recoverer)
	// Basic CORS
	// for more ideas, see: https://developer.github.com/v3/#cross-origin-resource-sharing
	router.Use(cors.GetCubzhCORSHandler())

	router.Get("/", index)
	router.Get("/.well-known/assetlinks.json", androidAssetLinks)
	router.Get("/.well-known/apple-app-site-association", appleAppSiteAssociation)
	router.Get("/{file}", staticFile)
	router.Get("/editions/{edition}/", editionIndex)
	router.Get("/editions/{edition}/{file}", editionStaticFile)
	router.Get("/ping", ping)

	listenAddr := LISTEN_ADDR
	if useSecureMode {
		listenAddr = LISTEN_ADDR_TLS
	}

	fmt.Println("🚀 Launching server...", listenAddr)

	if useSecureMode {
		err = http.ListenAndServeTLS(listenAddr, tlsCertPath, tlsKeyPath, router)
	} else {
		err = http.ListenAndServe(listenAddr, router)
	}

	fmt.Println("ERROR:", err.Error())
}

func ping(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(http.StatusOK)
	w.Write([]byte("pong"))
}

func index(w http.ResponseWriter, r *http.Request) {
	script := r.URL.Query().Get("script")
	if len(script) <= 0 {
		serveFile(LATEST_DIR+"cubzh.html", w, r)
		return
	}

	parts := strings.SplitN(script, "/", 3)
	if len(parts) < 3 {
		http.Error(w, "Invalid script format", http.StatusBadRequest)
		return
	}
	repoPart := parts[2]
	repoParts := strings.SplitN(repoPart, ":", 2)
	repo := repoParts[0]
	branchOrHash := "main"
	if len(repoParts) > 1 {
		branchOrHash = repoParts[1]
	}

	// Fetch the cubzh.json file from the GitHub repository
	url := fmt.Sprintf("https://raw.githubusercontent.com/%s/%s/%s/cubzh.json", parts[1], repo, branchOrHash)
	resp, err := http.Get(url)
	if err != nil || resp.StatusCode != http.StatusOK {
		serveFile(LATEST_DIR+"cubzh.html", w, r)
		return
	}
	defer resp.Body.Close()

	// Read and parse the JSON content
	var data map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
		http.Error(w, "Error parsing JSON", http.StatusInternalServerError)
		return
	}

	// Check for the EDITION value
	if env, ok := data["env"].(map[string]interface{}); ok {
		if edition, ok := env["EDITION"].(string); ok {
			queryParams := r.URL.RawQuery
			http.Redirect(w, r, fmt.Sprintf("/editions/%s/?%s", edition, queryParams), http.StatusFound)
			return
		}
	}
	serveFile(LATEST_DIR+"cubzh.html", w, r)
}

func editionIndex(w http.ResponseWriter, r *http.Request) {
	serveFile(EDITION_DIR+chi.URLParam(r, "edition")+"/cubzh.html", w, r)
}

func appleAppSiteAssociation(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	fmt.Fprintf(w, `{
  "applinks": {
      "details": [
           {
             "appIDs": [ "9JFN8QQG65.com.voxowl.particubes.macos", "9JFN8QQG65.com.voxowl.particubes.ios" ],
             "components": [
               {
                "/": "/",
               },
               {
                "/": "",
               },
               {
                "/": "/",
                "?": { "worldID": "*" }
               },
               {
                "/": "",
                "?": { "worldID": "*" }
               },
             ]
           }
       ]
   },
}`)
}

func androidAssetLinks(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	fmt.Fprintf(w, `[{
		"relation": ["delegate_permission/common.handle_all_urls"],
		"target": {
			"namespace": "android_app",
			"package_name": "com.voxowl.pcubes.android",
			"sha256_cert_fingerprints":
			[
				"82:9E:5F:6E:B3:0C:E7:4B:A0:3A:31:6B:32:6C:EB:AB:A1:95:33:6B:2B:85:98:64:7C:C1:5E:36:41:BD:80:4E",
				"A1:00:06:30:C4:C4:8F:FB:D2:20:1F:00:38:4A:DF:53:9C:74:F9:5A:F3:A9:07:F2:B2:C8:D2:47:77:B6:31:1B"
			]
		}
	}]`)
	// 1st key = debug key (SHA256 from voxowl_debug.jks)
	// 2nd key = release key (from Google Play Console auto-generated Asset Links JSON file)
}

func staticFile(w http.ResponseWriter, r *http.Request) {
	name := LATEST_DIR + chi.URLParam(r, "file")

	// it's ok to support .gz extension to benefit from Cloudflare's cache
	// removing it dynamically to get to the real file.
	name = strings.TrimSuffix(name, ".gz")

	serveFile(name, w, r)
}

func editionStaticFile(w http.ResponseWriter, r *http.Request) {
	edition := chi.URLParam(r, "edition")
	name := EDITION_DIR + edition + "/" + chi.URLParam(r, "file")

	// it's ok to support .gz extension to benefit from Cloudflare's cache
	// removing it dynamically to get to the real file.
	name = strings.TrimSuffix(name, ".gz")

	serveFile(name, w, r)
}

func extractDomainElements(host string) []string {
	// Remove port if present
	host = strings.Split(host, ":")[0]

	// Split the host by dot
	parts := strings.Split(host, ".")

	return parts
}

func serveFile(path string, w http.ResponseWriter, r *http.Request) {
	var err error

	w.Header().Set("Cross-Origin-Resource-Policy", "cross-origin") // CORP
	w.Header().Set("Cross-Origin-Embedder-Policy", "require-corp") // COEP
	w.Header().Set("Cross-Origin-Opener-Policy", "same-origin")    // COOP

	// w.Header().Set("Cache-Control", "public, max-age=3600, immutable") // 1h
	w.Header().Set("Cache-Control", "no-cache") // no cache

	// Construct path of file to serve
	if !filepath.IsAbs(path) {
		path, err = filepath.Abs(path)
		if err != nil {
			fmt.Println("ERROR:", err.Error())
			w.WriteHeader(http.StatusInternalServerError)
			return
		}
	}

	// fmt.Println("🔍 SERVE FILE:", path)

	mtx.Lock()
	f, found := files[path]
	mtx.Unlock()
	if !found || !useInMemoryCache {
		err = loadFile(path)
		if err != nil {
			fmt.Println("🔥", http.StatusNotFound, "|", path)
			w.WriteHeader(http.StatusNotFound)
			return
		}
		mtx.Lock()
		f, found = files[path]
		mtx.Unlock()
		if !found {
			fmt.Println("🔥🔥🔥", http.StatusNotFound, "|", path)
			w.WriteHeader(http.StatusNotFound)
			return
		}
	}

	etag := r.Header.Get("If-None-Match")

	if etag != "" && etag == f.Etag {
		// fmt.Println(http.StatusNotModified, "|", name)
		w.WriteHeader(http.StatusNotModified)
		return
	}

	w.Header().Set("Content-Type", f.ContentType)
	w.Header().Set("ETag", f.Etag)

	var reader *bytes.Reader

	if !strings.Contains(r.Header.Get("Accept-Encoding"), "gzip") {
		w.Header().Set("Content-Length", f.ContentLength)
		reader = bytes.NewReader(f.Bytes)

		fmt.Println(http.StatusOK, "|", path, "(len: "+f.ContentLength+", etag:"+f.Etag+")")
	} else {
		w.Header().Set("Content-Encoding", "gzip")
		w.Header().Set("Content-Length", f.GZippedContentLength)
		reader = bytes.NewReader(f.GZipped)

		// fmt.Println(http.StatusOK, "|", path, "(len: "+f.GZippedContentLength+" (gzip), etag:"+f.Etag+")")
	}

	w.WriteHeader(http.StatusOK)
	io.Copy(w, reader)
}

func loadFile(path string) error {
	ext := filepath.Ext(path)
	contentType := mime.TypeByExtension(ext)

	b, err := os.ReadFile(path)
	if err != nil {
		return err
	}

	reader := bytes.NewReader(b)
	hash := md5.New()
	_, err = io.Copy(hash, reader)
	if err != nil {
		fmt.Println("can't create md5")
		return err
	}

	md5Str := fmt.Sprintf("%x", hash.Sum(nil))

	var buf bytes.Buffer
	w := gzip.NewWriter(&buf)
	w.Write(b)
	w.Close()

	gzipped := buf.Bytes()

	bytesLen := len(b)
	gzippedLen := len(gzipped)

	f := File{
		Bytes:                b,
		Etag:                 md5Str,
		ContentType:          contentType,
		ContentLength:        strconv.FormatInt(int64(bytesLen), 10),
		GZipped:              gzipped,
		GZippedContentLength: strconv.FormatInt(int64(gzippedLen), 10),
		Path:                 path,
	}
	mtx.Lock()
	files[path] = f
	mtx.Unlock()

	fmt.Printf("loaded: %s, %s, %.3fMB, gzipped: %.3fMB\n", path, md5Str, toMB(bytesLen), toMB(gzippedLen))

	return nil
}

func toMB(nbBytes int) float32 {
	return float32(nbBytes) / (1 << 20)
}

// func logRequestHeaders(r *http.Request) {
// 	for k, v := range r.Header {
// 		fmt.Println(k, ":", v)
// 	}
// }
