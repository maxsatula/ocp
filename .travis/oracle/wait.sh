#!/bin/sh -e

docker logs --follow $(docker ps -lq) &
tail_pid=$!

until docker logs $(docker ps -lq) | grep "#########################"
do
    sleep 15
done

kill $tail_pid
