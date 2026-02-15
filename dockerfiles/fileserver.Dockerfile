# context is root of the git repo

# --------------------------------------------------
# Build Go HTTP server
# --------------------------------------------------

FROM golang:1.23.4-alpine3.21 AS builder

COPY ./go/cu.bzh/cors /go/cu.bzh/cors
COPY ./go/cu.bzh/fileserver /go/cu.bzh/fileserver

WORKDIR /go/cu.bzh/fileserver

RUN go build -o server

CMD ["ash"]

# --------------------------------------------------
# HTTP server runner
# --------------------------------------------------
FROM alpine:3.21 AS runner

# get the http server executable
COPY --from=builder /go/cu.bzh/fileserver/server /server

EXPOSE 80

CMD ["/server"]
