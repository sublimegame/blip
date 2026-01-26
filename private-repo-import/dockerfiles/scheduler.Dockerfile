#
# TODO: gdevillele: optimize this the same way as hub-server.Dockerfile
#
# -------------------------------------
# Compile scheduler server (Go)
# -------------------------------------
# binary created at /go/cu.bzh/scheduler/exe
FROM golang:1.24.2-alpine3.21 AS go-build-scheduler

COPY ./go/cu.bzh/scheduler /go/cu.bzh/scheduler
# COPY ./go/cu.bzh/types/gameserver /go/cu.bzh/types/gameserver
WORKDIR /go/cu.bzh/scheduler

RUN go build -o exe *.go

# -------------------------------------
# Scheduler server image
# -------------------------------------
FROM alpine:3.21 AS scheduler

# install executable
COPY --from=go-build-scheduler /go/cu.bzh/scheduler/exe /scheduler

# install config JSON
COPY ./servers/schedulers.json /schedulers.json

EXPOSE 80

CMD ["/scheduler"]
