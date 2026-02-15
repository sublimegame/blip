#!/bin/bash

docker build \
--platform linux/amd64 \
-t registry.digitalocean.com/cubzh/codegen-ollama-embed:latest \
.

docker push registry.digitalocean.com/cubzh/codegen-ollama-embed:latest
