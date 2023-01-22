FROM node:16

ARG UID=8008
ARG GID=8008

RUN set -x \
        && addgroup --system --gid $GID nginx || true \
    && adduser --system --disabled-login --ingroup nginx --no-create-home --home /nonexistent --gecos "nginx user" --shell /bin/false --uid $UID nginx || true

RUN DEBIAN_FRONTEND=noninteractive apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        nmap

WORKDIR /usr/src/app

COPY package.json /usr/src/app/
RUN npm install
COPY *.js /usr/src/app/
COPY .env /usr/src/app/ 
RUN mkdir cfg && echo "[]" > /usr/src/app/cfg/configured-clients.json && chown 8008:8008 /usr/src/app/cfg/configured-clients.json

EXPOSE 8008 8009
STOPSIGNAL SIGQUIT
USER $UID
CMD [ "node", "server.js" ]