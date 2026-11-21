#!/bin/bash
make || exit

Xephyr -screen 1600x900 +xinerama :80 &
sleep 0.1
export DISPLAY=:80
./shitwm

