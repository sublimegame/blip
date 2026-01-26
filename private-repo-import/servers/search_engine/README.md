# Search engine for items (and more)

## Typesense

Typesense is an opensource alternative to Algolia.

## Links

**Repository & Documentation**
https://github.com/typesense/typesense
https://typesense.org/docs/
https://hub.docker.com/r/typesense/typesense

**Go client library**
https://github.com/typesense/typesense-go

## Setup & test Typesense

```
docker pull typesense/typesense:0.24.0.rcn54

mkdir /data
docker run -p 8108:8108 -v data:/data typesense/typesense:0.24.0.rcn54 --data-dir /data --api-key=Hu52ddsas2AdxdE

# running on gaetan dev-1 machine:
# IPv4: 144.126.245.237 
# IPv6: 2a03:b0c0:3:d0::f13:9001
```
