####################################

FROM golang:1.23.4-alpine3.21 AS build-env

# copy Go packages needed
COPY ./go/cu.bzh/web/link /go/cu.bzh/web/link
COPY ./go/cu.bzh/amplitude /go/cu.bzh/amplitude
COPY ./go/cu.bzh/utils /go/cu.bzh/utils

WORKDIR /go/cu.bzh/web/link

####################################

FROM build-env AS builder

RUN go build

####################################

FROM alpine:3.21 AS runner

COPY --from=builder /go/cu.bzh/web/link/link /link

EXPOSE 80

ENTRYPOINT ["/link"]
