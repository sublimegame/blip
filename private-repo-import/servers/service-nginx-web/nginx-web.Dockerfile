FROM --platform=amd64 nginx:1.27.3-alpine

# NGINX configuration
COPY ./nginx.conf /etc/nginx/nginx.conf
# COPY ./default.conf /etc/nginx/conf.d/default.conf
# COPY ./fastcgi.conf /etc/nginx/fastcgi.conf

# Cubzh SSL/TLS certifications are expected to be mounted here:
# /cubzh/certificates
