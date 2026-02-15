package http

import (
	"net"
	"net/http"
	"strings"
)

func GetIP(r *http.Request) string {

	// Check if behind a proxy
	forwardedFromIP := r.Header.Get("X-Forwarded-For")
	if forwardedFromIP != "" {
		// X-Forwarded-For is potentially a list of addresses separated by ","
		// example:
		// X-Forwarded-For: client, proxy1, proxy2
		return strings.Split(forwardedFromIP, ",")[0]
	}

	// Standard way to get the IP if not behind a proxy
	ip, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return "" // or return an appropriate error
	}
	return ip
}
