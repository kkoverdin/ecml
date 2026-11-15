#!/usr/bin/env bash
sudo ip link add veth0 type veth peer name veth1 2>/dev/null || true
sudo ip link set veth0 up
sudo ip link set veth1 up
