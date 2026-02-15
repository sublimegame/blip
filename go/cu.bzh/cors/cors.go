package cors

import (
	"fmt"
	"net/http"
	"strings"

	"github.com/go-chi/cors"
)

const (
	HTTP_HEADER_CUBZH_APP_VERSION_V2     = "Czh-Ver"    // introduced on 0.0.48 patch 2
	HTTP_HEADER_CUBZH_APP_BUILDNUMBER_V2 = "Czh-Rev"    // introduced on 0.0.48 patch 2
	HTTP_HEADER_CUBZH_USER_ID            = "vx_userid"  // user credentials // TODO: gaetan: replace with a canonical form "Czh-Usr-Id"
	HTTP_HEADER_CUBZH_USER_TOKEN         = "vx_token"   // user credentials // TODO: gaetan: replace with a canonical form "Czh-Usr-Token"
	HTTP_HEADER_CUBZH_USER_ID_V2         = "Czh-Usr-Id" // introduced on 0.0.62
	HTTP_HEADER_CUBZH_USER_TOKEN_V2      = "Czh-Tk"     // introduced on 0.0.62
	HTTP_HEADER_IAP_SANDBOX_KEY          = "Blip-Iap-Sandbox-Key"
)

var (
	CORS_ALLOWED_METHODS = []string{
		"GET",
		"POST",
		"OPTIONS",
		"PATCH",
		"DELETE"}

	CORS_ALLOWED_HEADERS = []string{
		HTTP_HEADER_CUBZH_APP_VERSION_V2,
		HTTP_HEADER_CUBZH_APP_BUILDNUMBER_V2,
		HTTP_HEADER_CUBZH_USER_ID,
		HTTP_HEADER_CUBZH_USER_TOKEN,
		HTTP_HEADER_CUBZH_USER_ID_V2,
		HTTP_HEADER_CUBZH_USER_TOKEN_V2,
		HTTP_HEADER_IAP_SANDBOX_KEY,
		"session",
		"key",
		"Cache-Control",
		"If-None-Match",
		"X-Requested-With"}
)

func GetCubzhCORSHandler() func(http.Handler) http.Handler {
	return cors.Handler(cors.Options{
		AllowOriginFunc: func(r *http.Request, origin string) bool {
			fmt.Println("⭐️ ORIGIN:", origin)

			// allow dev environments
			if origin == "https://localhost:1443" || strings.Contains(origin, "://192.168.") {
				fmt.Println("⭐️   OK (dev environment)")
				return true
			}

			// exact matches
			if origin == "https://app.cu.bzh" {
				fmt.Println("⭐️   OK (exact match)")
				return true
			}

			// dynamic matching
			if strings.HasPrefix(origin, "https://") && strings.HasSuffix(origin, ".cu.bzh") {
				fmt.Println("⭐️   OK (dynamic match) (1)")
				return true // dynamic match
			}
			if strings.HasPrefix(origin, "https://") && strings.HasSuffix(origin, ".hf.space") {
				fmt.Println("⭐️   OK (dynamic match) (2)")
				return true
			}
			if strings.HasPrefix(origin, "https://") && strings.Contains(origin, "huggingface.co") {
				fmt.Println("⭐️   OK (dynamic match) (3)")
				return true
			}
			// https://1219341016524525670.discordsays.com
			if strings.HasPrefix(origin, "https://") && strings.Contains(origin, "discordsays.com") {
				fmt.Println("⭐️   OK (dynamic match) (4)")
				return true
			}

			fmt.Println("⭐️   NOT OK ❌")
			return false
		},
		AllowedMethods:   CORS_ALLOWED_METHODS,
		AllowedHeaders:   CORS_ALLOWED_HEADERS,
		AllowCredentials: false,
		MaxAge:           300, // Maximum value not ignored by any of major browsers
	})
}
