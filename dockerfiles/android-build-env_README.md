# Android build environment in Docker

## Build

```bash
cd particubes-private/dockerfiles

docker build -t android-build-env \
-f android-build-env.Dockerfile \
.
```

TODO: use arguments
```bash
docker build -t android-build-env \
--build-arg VX_NDK_VERSION=21.4.7075529 VX_ANDROID_API=30 \
-f android-build-env.Dockerfile \
.
```

Repeat this procedure for `android-build-openssl`

Note: we leave default tag `latest`

## Run

For debug,
```
docker run -ti --rm android-build-<env/openssl> bash
```

To export `openssl` libs,
```
docker run -v <...>/particubes-private/deps/libssl/android:/export --rm android-build-openssl
```

## Enjoy

...