# Hub Server

the hub server machine runs
- a front server (NGINX) to dispatch HTTP requests coming for different domains to the correct docker containers
- a docker container for http://api.particubes.com (unsecure)
    - container port :80 is for public Hub API (used by game clients)
    - container port :10080 is for private Hub API (used by gameservers)
- a docker container for https://api.particubes.com (SSL/TLS)
    - container port :443 is for public Hub API (used by game clients)
    - container port :10080 is for private Hub API (used by gameservers)
- a docker container for https://api.cu.bzh (SSL/TLS) with ports :443 and :1443
    - container port :443 is for public Hub API
    - container port :1443 is for private Hub API

The Hub server physical machine has the following files:

```shell
# server cert + certification authority cert
/cubzh/certificates/particubes.com.chained.crt
# server private key
/cubzh/certificates/particubes.com.key
# server cert + certification authority cert
/cubzh/certificates/cu.bzh.chained.crt
# server private key
/cubzh/certificates/cu.bzh.key

# nginx config file
/cubzh/config/nginx.conf
```

## NGINX Container

Note : the `nginx.conf` config file is manually installed on the physical server. (`/config/nginx/nginx.conf`)

### Configuration

This is an explanation of the nginx.conf file content.

...

### Run it manually (outside of Docker Compose)

```shell
# nginx arg -V is for enabling [SNI](https://en.wikipedia.org/wiki/Server_Name_Indication)
docker run --name my-custom-nginx-container -v /config/nginx/nginx.conf:/etc/nginx/nginx.conf:ro -d nginx:1.23.1-alpine

# or in debug mode
docker run --name my-custom-nginx-container -v /config/nginx/nginx.conf:/etc/nginx/nginx.conf:ro -d nginx:1.23.1-alpine nginx-debug -g 'daemon off;'
```

## Hub server container (api.cu.bzh)

This Hub API server only serves the API over HTTPS (SSL/TLS), and not over unsecure HTTP.
The Hub public API is served on the container port :443.
The Hub private API (used by gameservers) is served on the container port :1443.
