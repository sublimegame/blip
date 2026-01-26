# --------------------------------
# Build the bundle generator
# --------------------------------

FROM alpine:3.21 AS builder

RUN apk update && apk add --no-cache \
	build-base \
	gcc \
	cmake \
	make \
	&& rm -rf /var/cache/apk/*

COPY ./bin_to_c /bin_to_c

WORKDIR /bin_to_c

RUN make

# --------------------------------
# Generate the bundle
# --------------------------------

FROM alpine:3.21 AS bintoc

COPY --from=builder /bin_to_c/bin_to_c /bin_to_c
COPY ./build.sh /build.sh

# Expected directories to be mounted:
# /cubzh/bundle -> /src/bundle
# /cubzh/i18n -> /src/i18n
# /cubzh/lua/modules -> /src/modules

WORKDIR /

ENTRYPOINT ["/build.sh"]
