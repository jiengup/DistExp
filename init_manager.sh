#!/bin/bash 
REDIS_VERSION=6.2.6
REDIS_PASSWORD=repcacheec

set -euo pipefail

if [ ! -d redis ]; then
    echo "Redis not found. Downloading..."
    wget https://download.redis.io/releases/redis-${REDIS_VERSION}.tar.gz 
    tar xvf redis-${REDIS_VERSION}.tar.gz 
    mv redis-${REDIS_VERSION} redis;
    pushd redis
    make -j
    # allow connection
    sed -i "/bind 127.0.0.1 -::1/d" redis.conf
    # set password
    sed -i "s/# requirepass foobared/requirepass ${REDIS_PASSWORD}/g" redis.conf
    popd
    rm redis-${REDIS_VERSION}.tar.gz
fi 

screen -S redis -dm ./redis/src/redis-server ./redis/redis.conf
sleep 2
./redis/src/redis-cli -a ${REDIS_PASSWORD}
