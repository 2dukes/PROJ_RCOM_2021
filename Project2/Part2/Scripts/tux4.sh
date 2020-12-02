#!/bin/bash

# UpdateImage
# /etc/init.d/networking restart

ifconfig eht0 up
ifconfig eht1 up
ifconfig eth0 172.16.10.254/24
ifconfig eth1 172.16.11.253/24
route -n

# Exp4
route add default gw 172.16.11.254
route -n