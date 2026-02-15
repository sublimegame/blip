#
# USAGE:
#
# docker build -t tracking-dev -f ./servers/tracking.Dockerfile .
#
# docker run -p 8080:80 -e CZH_TRACK_CHANNEL=dev tracking-dev
# docker run -p 8080:80 -e CZH_TRACK_CHANNEL=prod tracking-dev
#

FROM golang:1.23.4-alpine3.21 AS build-env

# Go packages needed
COPY /go/cu.bzh/tracking /go/cu.bzh/tracking
COPY /go/cu.bzh/utils/http /go/cu.bzh/utils/http
COPY /go/cu.bzh/cors /go/cu.bzh/cors
COPY /go/cu.bzh/amplitude /go/cu.bzh/amplitude

#################################

FROM build-env AS builder

COPY --from=build-env /go /go

WORKDIR /go/cu.bzh/tracking

RUN go build

#################################

FROM alpine:3.21 AS runner

# get the compiled binary
COPY --from=builder /go/cu.bzh/tracking/tracking /tracking

CMD ["/tracking"]
