FROM openresty/openresty
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        openresty-opm openssl \
    && opm get ledgetech/lua-resty-http
RUN mkdir -p /etc/ssl/certs/server/selfsigned && \
    mkdir -p /etc/ssl/certs/server/ca && \
    openssl genrsa -aes256 -passout pass:kZvq9IVp16ERAsb3Y4dAZCJ2Ik5K2GNb7IwW7vswV0yBNInLeb -out /etc/ssl/private/ca.pass.key 4096 && \
    openssl rsa -passin pass:kZvq9IVp16ERAsb3Y4dAZCJ2Ik5K2GNb7IwW7vswV0yBNInLeb -in /etc/ssl/private/ca.pass.key -out /etc/ssl/private/ca.key && \
    openssl req -new -x509 -days 365 -key /etc/ssl/private/ca.key -out /etc/ssl/certs/server/ca/ca.crt \
                     -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=IT Department/CN=example.com" && \
    openssl genrsa -aes256 -passout pass:kZvq9IVp16ERAsb3Y4dAZCJ2Ik5K2GNb7IwW7vswV0yBNInLeb -out /etc/ssl/private/server.pass.key 4096 && \
    openssl rsa -passin pass:kZvq9IVp16ERAsb3Y4dAZCJ2Ik5K2GNb7IwW7vswV0yBNInLeb -in /etc/ssl/private/server.pass.key -out /etc/ssl/private/server.key && \
    openssl req -new -key /etc/ssl/private/server.key -out /etc/ssl/private/server.csr \
                -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=IT Department/CN=example.com" && \
    openssl req -new -x509 -days 365 -in /etc/ssl/private/server.csr -key /etc/ssl/private/ca.key -out /etc/ssl/certs/server/selfsigned/server.crt \
                -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=IT Department/CN=example.com" && \
    rm /etc/ssl/private/server.pass.key && \
    openssl dhparam -out /etc/nginx/dhparam.pem 4096
COPY scripts/bakeClientCert.sh /usr/bin/bakeClientCert.sh
RUN chmod +x /usr/bin/bakeClientCert.sh

    