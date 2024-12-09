#! /bin/bash

timeout=60

function general_reset()
{
	echo "[INFO] Reseting interfaces to default"

    #echo "Restart networking"
    #systemctl restart networking

    echo "Disable interfaces"
    ifconfig eth0 down	
    ifconfig eth1 down
	ifconfig eth2 down

    echo "Delete all routes"
    ip route flush table main

    echo "Disable ignore broadcasts"
	sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
}

function setup_dns()
{
    echo "Setting up dns"
	echo "nameserver 10.227.20.3" > /etc/resolv.conf

    sleep 2

    echo "Stop systemd-resolved"
   	
    systemctl stop systemd-resolved
}

function network_setup_2()
{
	echo "[INFO] Setting up tux 2"

	ifconfig eth0 10.227.20."$1"2/24
	
	ifconfig eth1 172.16."$1"1.1/24

	route add -net 172.16."$1"0.0/24 gw 172.16."$1"1.254
	route add -net 172.16.1.0/24 gw 172.16."$1"1.254

	# Default router (Rc)
	route add default gw 172.16."$1"1.254
	route add default gw 10.227.20.254

	# Deletes ARP cache
	ip neigh flush all

    setup_dns
}

function network_setup_3()
{
	echo "[INFO] Setting up tux 3"

	ifconfig eth0 10.227.20."$1"3/24
	ifconfig eth1 172.16."$1"0.1/24
	
	route add -net 172.16."$1"1.0/24 gw 172.16."$1"0.254
	route add -net 172.16.1.0/24 gw 172.16."$1"0.254

	# Default router (Y4)
	route add default gw 172.16."$1"0.254
	route add default gw 10.227.20.254

	# Deletes ARP cache
	ip neigh flush all

    setup_dns
}

function network_setup_4()
{
	echo "[INFO] Setting up tux 4"

	sysctl net.ipv4.ip_forward=1

	ifconfig eth0 10.227.20."$1"4/24
	ifconfig eth1 172.16."$1"0.254/24
	ifconfig eth2 172.16."$1"1.253/24

	route add -net 172.16.1.0/24 gw 172.16."$1"0.254

	# Default router (Rc)
	route add default gw 172.16."$1"1.254
	route add default gw 10.227.20.254

	# Deletes ARP cache
	ip neigh flush all

    setup_dns
}

function microtik_setup()
{
    echo "Setting up MicroTik"

	stty -F /dev/ttyS0 115200 cs8 -cstopb -parenb
	
	echo -e "/system reset-configuration\r\n" > /dev/ttyS0
	echo -e "y\r\n" > /dev/ttyS0
    
    echo "Starting reset timeout (60s). Press enter if reset already finished... "
	
	elapsed_time=0

	while [ $elapsed_time -lt $timeout ]; do
		sleep 1

		elapsed_time=$((elapsed_time + 1))
		
		if read -t 0; then
   			break
		fi
	done

    echo "Ended reset timeout"

	echo -e "admin\r\n" > /dev/ttyS0
	sleep 1

	echo -e "\r\n" > /dev/ttyS0
	sleep 1

	echo -e "\r\n" > /dev/ttyS0
	sleep 1

    echo "Reset and login success"
}

function switch_port_remove()
{
	echo -e '/interface bridge port remove [find interface=ether'"$1"']\r\n' > /dev/ttyS0
	sleep 1
}

function switch_bridge_add()
{
	echo -e '/interface bridge add name=bridge'"$1""$2"'\r\n' > /dev/ttyS0
	sleep 1
}

function switch_port_add()
{
	echo -e '/interface bridge port add bridge=bridge'"$1""$2"' interface=ether'"$3"'\r\n' > /dev/ttyS0
	sleep 1
}

function switch_setup()
{
	echo
	echo "[INFO] This script expects the following connections:"
	echo "          - TuxY2 / E1 => ether8"
	echo "          - TuxY3 / S0 => Switch Console"
	echo "          - TuxY3 / E1 => ether14"
	echo "          - TuxY4 / E1 => ether18"
	echo "          - TuxY4 / E2 => ether22"
	echo "          - Rc    / E2 => ether1"
	echo
	read -p "[WARN] Press enter to start switch configuration... "

	microtik_setup

	echo "Create Bridge 0"
	switch_bridge_add "$1" 0
	echo "Create Bridge 1"
	switch_bridge_add "$1" 1

	echo "Create Y3/0 - ether14"
	switch_port_remove 14
	switch_port_add "$1" 0 14

	echo "Create Y4/0 - ether18"
	switch_port_remove 18
	switch_port_add "$1" 0 18
	
	echo "Create Y4/1 - ether22"
	switch_port_remove 22
	switch_port_add "$1" 1 22
	
	echo "Create Y2/1 - ether8"
	switch_port_remove 8
	switch_port_add "$1" 1 8

	echo "Create Rc/1 - ether1"
	switch_port_remove 1
	switch_port_add "$1" 1 1
}

function router_setup()
{
	echo
	echo "[INFO] This script expects the following connections:"
	echo "          - Rc    / E1 => ""$1"".12"
	echo "          - TuxY3 / S0 => MT Console"
	echo
	read -p "[WARN] Press enter to start router configuration... "

	microtik_setup

	echo "Set Internal IP"
	echo -e '/ip address add address=172.16.'"$1"'1.254/24 interface=ether2\r\n' > /dev/ttyS0
	sleep 1

	echo "Set Lab Network IP"
	echo -e '/ip address add address=10.227.20.'"$1"'9/24 interface=ether1\r\n' > /dev/ttyS0
	sleep 1

    echo "Set 172.16.1.x IP"
	echo -e '/ip address add address=172.16.1.'"$1"'9/24 interface=ether1\r\n' > /dev/ttyS0
	sleep 1

    echo "Add route to 172.16.Y0.0"
	echo -e '/ip route add dst-address=172.16.'"$1"'0.0/24 gateway=172.16.'"$1"'1.253\r\n' > /dev/ttyS0
	sleep 1

    echo "Add route to internet"
	echo -e "/ip route add dst-address=0.0.0.0/0 gateway=10.227.20.254\r\n" > /dev/ttyS0
	sleep 1

	read -p "[WARN] Would you like to disable NAT? (y/n) " opt

	opt=${opt,,}
	if [[ "$opt" == "y" ]]; then
		echo -e "/ip firewall nat disable 0\r\n" > /dev/ttyS0
		sleep 1

		read -p "[WARN] Would you like to setup NAT? (y/n) " opt

		opt=${opt,,}
		if [[ "$opt" == "y" ]]; then
			echo -e "/ip firewall nat add chain=srcnat action=masquerade out-interface=ether1\r\n" > /dev/ttyS0
			sleep 1
		fi
	fi
}

if [ 15 -lt "$1" ] || [ 1 -gt "$1" ]; then
	printf "[ERRO] Invalid bench number.\nUsage: %s <bench number> <tux>\n" "$0"
	exit 1
fi

if [ 4 -lt "$2" ] || [ 2 -gt "$2" ]; then
	printf "[ERRO] Invalid tux number.\nUsage: %s <bench number> <tux>\n" "$0"
	exit 1
fi

general_reset

case "$2" in
	2)
		network_setup_2 "$1"
		;;
	3)
		network_setup_3 "$1"
		switch_setup "$1"
		router_setup "$1"
		;;
	4)
		network_setup_4 "$1"
		;;
esac
