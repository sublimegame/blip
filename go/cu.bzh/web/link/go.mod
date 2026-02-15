module cu.bzh/web/link

go 1.23.4

replace cu.bzh/amplitude v0.0.1 => ../../amplitude

replace cu.bzh/utils/http v0.0.1 => ../../utils/http

require (
	cu.bzh/amplitude v0.0.1
	cu.bzh/utils/http v0.0.1
	github.com/go-chi/chi/v5 v5.2.0
)
