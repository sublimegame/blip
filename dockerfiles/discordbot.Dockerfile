#################################

FROM golang:1.24.2-alpine3.21 AS build-env

# Go sources

# discordbot
# COPY ./go/cu.bzh /go/cu.bzh
COPY ./go/cu.bzh/discordbot /go/cu.bzh/discordbot

# deps
COPY ./go/cu.bzh/hub/apnsclient /go/cu.bzh/hub/apnsclient
COPY ./go/cu.bzh/hub/dbclient /go/cu.bzh/hub/dbclient
COPY ./go/cu.bzh/hub/fcmclient /go/cu.bzh/hub/fcmclient
COPY ./go/cu.bzh/hub/game /go/cu.bzh/hub/game
COPY ./go/cu.bzh/hub/item /go/cu.bzh/hub/item
COPY ./go/cu.bzh/hub/scyllaclient /go/cu.bzh/hub/scyllaclient
COPY ./go/cu.bzh/hub/search /go/cu.bzh/hub/search
COPY ./go/cu.bzh/hub/transactions /go/cu.bzh/hub/transactions
COPY ./go/cu.bzh/hub/types /go/cu.bzh/hub/types
COPY ./go/cu.bzh/hub/user /go/cu.bzh/hub/user
COPY ./go/cu.bzh/scheduler /go/cu.bzh/scheduler
COPY ./go/cu.bzh/types/gameserver /go/cu.bzh/types/gameserver
COPY ./go/cu.bzh/utils /go/cu.bzh/utils
COPY ./go/cu.bzh/web/link /go/cu.bzh/web/link

WORKDIR /go/cu.bzh/discordbot

#################################

FROM build-env AS builder

RUN go build

#################################

FROM alpine:3.21 AS runner

COPY --from=builder /go/cu.bzh/discordbot/discordbot /discordbot

EXPOSE 80

CMD ["/discordbot"]
