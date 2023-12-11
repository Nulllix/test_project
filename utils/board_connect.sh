#!/bin/sh

sudo modprobe bluetooth_6lowpan
echo "1" | sudo tee /sys/kernel/debug/bluetooth/6lowpan_enable
echo "connect F1:A8:90:93:FD:B7 2" | sudo tee /sys/kernel/debug/bluetooth/6lowpan_control
sleep 2
sudo ip address add 2001:db8::2/64 dev bt0
# sleep 2
# python3 test_conn.py

# sudo nc -6 -v -u 2001:db8::1 4242
# sudo nc -6 -v 2001:db8::1 4242
