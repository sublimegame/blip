FROM alpine:3.16.2 AS build-env

RUN apk update && apk add --no-cache \ 
	build-base \
	gcc \
	cmake \
	make \
	&& rm -rf /var/cache/apk/*

# /!\ CONTEXT MUST BE PROJECT ROOT DIR
# TO BUILD WITHOUT ISSUES.

COPY ./bin_to_c /bin_to_c

ENTRYPOINT ["ash"]

#################################

FROM alpine:3.16.2 AS builder

RUN apk update && apk add --no-cache \ 
	build-base \
	gcc \
	cmake \
	make \
	&& rm -rf /var/cache/apk/*

COPY --from=build-env /bin_to_c /bin_to_c

WORKDIR /bin_to_c
RUN make

ENTRYPOINT ["ash"]

#################################

FROM alpine:3.16.2 AS bintoc

# RUN apk update && apk add --no-cache \ 
#	libstdc++ \
#	&& rm -rf /var/cache/apk/*

COPY --from=builder /bin_to_c/bin_to_c /bin_to_c
# COPY ./build.sh /build.sh
# COPY ./16.rgba /input/16.rgba
# COPY ./32.rgba /input/32.rgba
# COPY ./48.rgba /input/48.rgba

WORKDIR /

ENTRYPOINT ["/input/build.sh"]
