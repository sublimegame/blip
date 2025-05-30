FROM alpine:3.21 AS audio-converter

RUN apk update && apk add --no-cache \
	ffmpeg \
	&& rm -rf /var/cache/apk/*

COPY ./bundle/audio /audio_input
RUN mkdir /audio_output

# WORKDIR /audio_input
RUN find /audio_input -name '*.ogg' -exec sh -c 'ffmpeg -i "$1" "/audio_output/$(basename "${1%.ogg}.mp3")"' _ {} \;

#################################

FROM --platform=linux/amd64 golang:1.24.1-alpine3.21 AS build-env

RUN apk update && apk add --no-cache \
	git \
	&& rm -rf /var/cache/apk/*

COPY ./lua/docs/webserver /webserver
COPY ./lua/docs/content /www
COPY ./lua/docs/parser /parser
COPY ./lua/modules /modules
COPY --from=audio-converter /audio_output /www/audio

RUN cd /parser && ./parse.sh

WORKDIR /webserver

EXPOSE 80

#################################

FROM --platform=linux/amd64 build-env AS builder

RUN go build

#################################
# This is like a production server but hot-refreshing
# of content is available for documentation-writing purposes.

FROM --platform=linux/amd64 alpine:3.21 AS website-dev

# then the executable
COPY --from=builder /webserver/webserver /webserver
COPY --from=builder /www /www
COPY --from=builder /modules /modules

EXPOSE 80
WORKDIR /

ENTRYPOINT ["/webserver"]

#################################
# This is a production webserver.
# Content is cached (content hot-refreshing is not available).

FROM --platform=linux/amd64 website-dev AS website

ENV RELEASE=1
