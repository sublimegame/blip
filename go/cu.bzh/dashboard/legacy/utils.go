package main

import "net/http"

func respond(code int, msg string, w http.ResponseWriter) {
	w.WriteHeader(code)
	w.Write([]byte(msg))
}
