module cu.bzh/wasmserver

go 1.24.1

replace cu.bzh/cors v0.0.1 => ../cors

require (
	cu.bzh/cors v0.0.1
	github.com/go-chi/chi/v5 v5.2.1
)

require github.com/go-chi/cors v1.2.1 // indirect
