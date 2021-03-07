#!/bin/bash
xhost +local:root
export DISPLAY=:0
docker start turtle
docker exec -it turtle bash
