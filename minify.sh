#!/bin/bash

cat ./res/src/main.js ./res/src/misc/*.js ./res/src/controls/*.js ./res/src/components/*.js ./res/src/app.js > ./res/dist/app-min.js
cat ./res/src/css/app.css > ./res/dist/app-min.css
