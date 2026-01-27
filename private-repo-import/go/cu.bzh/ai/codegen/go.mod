module cu.bzh/ai/codegen

go 1.24.3

replace blip.game/diff => ../../../blip.game/diff

replace blip.game/middleware => ../../../blip.game/middleware

replace cu.bzh/ai => ../

replace cu.bzh/ai/anthropic => ../anthropic

replace cu.bzh/ai/gemini => ../gemini

replace cu.bzh/ai/grok => ../grok

require (
	blip.game/middleware v0.0.0-00010101000000-000000000000
	cu.bzh/ai v0.0.0-00010101000000-000000000000
	cu.bzh/ai/anthropic v0.0.0-00010101000000-000000000000
	cu.bzh/ai/gemini v0.0.0-00010101000000-000000000000
	cu.bzh/ai/grok v0.0.0-00010101000000-000000000000
	github.com/go-chi/chi/v5 v5.2.1
	github.com/ollama/ollama v0.6.5
	gopkg.in/yaml.v2 v2.4.0
)

require (
	github.com/bcicen/jstream v1.0.1 // indirect
	github.com/kr/pretty v0.3.0 // indirect
	github.com/rogpeppe/go-internal v1.8.0 // indirect
	gopkg.in/check.v1 v1.0.0-20201130134442-10cb98267c6c // indirect
)
