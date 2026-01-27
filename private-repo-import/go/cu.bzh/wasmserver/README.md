# Particubes wasm - HTTP Server

## Container development

### build

```bash
# execute this from particubes-private git repository root directory
# --target flag can be used
docker build -t wasm -f ./dockerfiles/wasm.Dockerfile .
```

### Run

```bash
# to run from <repo root>/wasm
docker run --rm -p 8080:80 -p 1443:443 -v $(pwd)/Particubes/build/output:/www wasm
```
