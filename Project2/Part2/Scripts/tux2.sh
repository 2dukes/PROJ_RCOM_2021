#!/bin/bash

# UpdateImage
# /etc/init.d/networking restart

ifconfig eht0 up
ifconfig eth0 172.16.11.1/24
route -n

# Exp4
route add default gw 172.16.11.254
route -n