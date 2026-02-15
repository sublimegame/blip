# Go vendoring

## To update vendored packages

### define GOPATH env var

```
exports GOPATH=~/projects/particubes-private/servers/go
```

### delete vendor/ and vendor.conf

```
rm vendor.conf
rm -R vendor
```

### re-vendor everything

```
vndr init
```
