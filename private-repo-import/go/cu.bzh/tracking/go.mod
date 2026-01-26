module cu.bzh/tracking

go 1.23.4

replace cu.bzh/amplitude v0.0.1 => ../amplitude

replace cu.bzh/cors v0.0.1 => ../cors

replace cu.bzh/utils/http v0.0.1 => ../utils/http

require (
	cu.bzh/amplitude v0.0.1
	cu.bzh/cors v0.0.1
	cu.bzh/utils/http v0.0.1
	github.com/go-chi/chi/v5 v5.2.0
	github.com/perimeterx/marshmallow v1.1.5
)

require (
	github.com/go-chi/cors v1.2.1 // indirect
	github.com/josharian/intern v1.0.0 // indirect
	github.com/mailru/easyjson v0.7.7 // indirect
)
