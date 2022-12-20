FROM node:16

RUN DEBIAN_FRONTEND=noninteractive apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        nmap

WORKDIR /usr/src/app

COPY package.json /usr/src/app/
RUN npm install
COPY *.js /usr/src/app/
COPY .env /usr/src/app/ 

EXPOSE 8008 8009
CMD [ "node", "server.js" ]