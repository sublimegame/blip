package main

import (
	"fmt"
	"net/http"
)

func privateAPIClientFilterMiddleware(next http.Handler) http.Handler {

	var authorizedHosts = []string{
		"13.36.138.117",   // pcubes-gameservers-eu-2
		"3.16.3.53",       // pcubes-gameservers-us-1
		"52.221.203.84",   // pcubes-gameservers-sg-2
		"185.146.221.228", // Adrien's home
		"79.80.22.6",      // Gaetan's home (Vendee?)
		"90.12.176.247",   // Gaetan's home (LRSY)
		"146.70.174.198",  // Proton VPN (US-CA#206)
	}

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		host, _ := getHostAndPort(r)
		for _, h := range authorizedHosts {
			if h == host {
				next.ServeHTTP(w, r)
				return
			}
		}

		fmt.Println("⚠️ HACK? - PRIVATE API - request sender not authorized:", host, r.RequestURI)
		respondWithError(http.StatusUnauthorized, "", w)
	})
}
