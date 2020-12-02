#!/bin/bash

# UpdateImage
# /etc/init.d/networking restart

ifconfig eht0 up
ifconfig eth0 172.16.10.1/24
route -n

# Exp 3
route add -net 172.16.11.0/24 gw 172.16.10.254
route -n