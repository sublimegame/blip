module cu.bzh/fileserver

go 1.23.4

replace cu.bzh/cors v0.0.1 => ../cors

require (
	cu.bzh/cors v0.0.1
	github.com/go-chi/chi/v5 v5.2.0
)

require github.com/go-chi/cors v1.2.1 // indirect
