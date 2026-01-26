FROM --platform=linux/amd64 golang:1.23.4-alpine3.21 AS build-env

RUN apk update && apk add --no-cache \
	git \
	&& rm -rf /var/cache/apk/*

# copy Go packages needed
COPY go /go

EXPOSE 80

WORKDIR /go/cu.bzh/dashboard

ENTRYPOINT ["ash"]

#################################

FROM --platform=linux/amd64 golang:1.23.4-alpine3.21 AS builder

RUN apk update && apk add --no-cache \
	git \
	&& rm -rf /var/cache/apk/*

COPY --from=build-env /go /go

WORKDIR /go/cu.bzh/dashboard

RUN go build

EXPOSE 80

ENTRYPOINT ["ash"]

#################################

FROM --platform=linux/amd64 alpine:3.21 AS dashboard

RUN apk update && apk add --no-cache \
	ca-certificates \
	&& rm -rf /var/cache/apk/*

COPY --from=builder /go/cu.bzh/dashboard/dashboard /dashboard
COPY --from=builder /go/cu.bzh/dashboard/dashboard.html /dashboard.html

EXPOSE 80

WORKDIR /

ENV GIN_MODE release

ENTRYPOINT ["/dashboard"]
