FROM golang:1.23.4-alpine3.21 AS build-env

# TODO: gdevillele: we should not need git anymore (since we don't use VNDR anymore)
RUN apk update && apk add --no-cache \
	git \
	&& rm -rf /var/cache/apk/*

# copy web resources from voxowl/particubes repository
COPY public/particubes-dot-com/content/_templates /assets/templates
COPY public/particubes-dot-com/content/style /assets/style
COPY public/particubes-dot-com/content/img /assets/img
COPY public/particubes-dot-com/content/js /assets/js
COPY public/particubes-dot-com/content/video /assets/video
COPY public/particubes-dot-com/content/pages /assets/pages
COPY servers/web-private-files /private

# copy Go packages needed
COPY ./go/cu.bzh /go/cu.bzh

EXPOSE 80

WORKDIR /go/cu.bzh/webserver

ENTRYPOINT ["ash"]

#################################

FROM golang:1.23.4-alpine3.21 AS builder

# TODO: gdevillele: we should not need git anymore (since we don't use VNDR anymore)
RUN apk update && apk add --no-cache \
	git \
	&& rm -rf /var/cache/apk/*

COPY --from=build-env /go /go
COPY --from=build-env /assets /assets
COPY --from=build-env /private /private

WORKDIR /go/cu.bzh/webserver

ENV CGO_ENABLED 0
ENV GIN_MODE release
# TODO: remove this. We use Go Modules now
# ENV GO111MODULE off

RUN go build

EXPOSE 80

ENTRYPOINT ["ash"]
# ENTRYPOINT ["./webserver"]

#################################

FROM alpine:3.21 AS website

RUN apk update && apk add --no-cache \
	ca-certificates \
	&& rm -rf /var/cache/apk/*

COPY --from=builder /go/cu.bzh/webserver/webserver /webserver
COPY --from=builder /assets /assets
COPY --from=builder /private /private

EXPOSE 80
WORKDIR /

ENV GIN_MODE release

VOLUME /cache

ENTRYPOINT ["/webserver"]
