FROM golang:1.24-alpine

# Go source code
COPY go/cu.bzh/ai/codegen /go/cu.bzh/ai/codegen
COPY go/cu.bzh/ai /go/cu.bzh/ai
COPY go/blip.game/diff /go/blip.game/diff
COPY go/blip.game/middleware /go/blip.game/middleware

WORKDIR /go/cu.bzh/ai/codegen

# RUN go build -o codegen cmd/server/main.go
RUN go build -o codegen

EXPOSE 80

ENV CODEGEN_CHROMADB_HOST=codegen-chromadb
ENV CODEGEN_CHROMADB_PORT=8000

CMD ["./codegen", "--serve"]
