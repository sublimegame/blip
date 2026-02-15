# --------------------------------------------------
# Build Go HTTP server
# --------------------------------------------------

FROM golang:1.23.4-alpine3.21 AS builder

# context is root of the git repo
COPY ./misc/parent-dashboard /parent-dashboard

WORKDIR /parent-dashboard

RUN go build main.go

# --------------------------------------------------
# Web server
# --------------------------------------------------

FROM alpine:3.21 AS web_server_empty

# get the http server executable
COPY --from=builder /parent-dashboard/main /server/parent-dashboard

# get resources files
COPY --from=builder /parent-dashboard/static /server/static
COPY --from=builder /parent-dashboard/templates /server/templates

EXPOSE 80

WORKDIR /server

CMD ["./parent-dashboard"]
