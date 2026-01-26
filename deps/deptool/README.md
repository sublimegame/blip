# Cubzh Dependencies Tool (deptool)

## Basic usage

### Download a dependency

```sh
# deptool download <dependency> <version> <platform>
deptool download libluau 0.665 macos
deptool download libluau 0.665 ios
deptool download libluau 0.665 android
deptool download libluau 0.665 windows
deptool download libluau 0.665 linux
```

### Make downloaded dependency version active

```sh
# deptool activate <dependency> <version>
deptool activate libluau 0.665
deptool activate libluau 0.665
deptool activate libluau 0.665
deptool activate libluau 0.665
deptool activate libluau 0.665
```

## Advanced usage

### Setup access key (required)

Add these lines to your `~/.bashrc` or `~/.zshrc` file, or set them in the `.env` file:

```bash
# Get these values from the .env file or team secrets manager
export DIGITALOCEAN_SPACES_AUTH_KEY="<your_auth_key>"
export DIGITALOCEAN_SPACES_AUTH_SECRET="<your_auth_secret>"
```

## Upload dependencies

```bash
# upload a dependency
# deptool upload <dependency> <platform> [<version>]
deptool upload libluau 0.665 android
deptool upload libluau 0.665 ios
deptool upload libluau 0.665 macos
deptool upload libluau 0.665 windows
deptool upload libluau 0.665 linux
```

## Build deptool

### Build for current platforms

```bash
# /!\ execute from the "deps/deptool/cmd" directory

# macos (arm64)
go build -o deptool_macos_arm64

# windows (x86_64)
go build -o deptool_windows_x86_64
```

## Build in docker container

```bash
# /!\ execute from the "deps/deptool/cmd" directory

docker run --platform linux/amd64 --rm -it -v $(pwd)/..:/deptool -w /deptool/cmd golang:1.24.1-alpine3.21 go build -o deptool_linux_x86_64
docker run --platform linux/arm64 --rm -it -v $(pwd)/..:/deptool -w /deptool/cmd golang:1.24.1-alpine3.21 go build -o deptool_linux_arm64
```
