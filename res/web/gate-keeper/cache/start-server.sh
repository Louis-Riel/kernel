#!/bin/bash

sed "s/REDIS_PWD/$REDIS_PWD/g" /etc/redis/redis-templ.conf > /etc/redis/redis.conf &&
redis-server /etc/redis/redis.conf