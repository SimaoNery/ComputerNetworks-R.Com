#!/bin/bash

user="root"
password="alanturing"

function reset_config() {
	sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.72 "systemctl restart networking"
	sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.73 "systemctl restart networking"
	sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.74 "systemctl restart networking"
}

function reset_microtik_switch() {
sshpass -p "alanturing" scp "$1" $user@10.227.20.73:/root/Desktop
sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.73 << 'EOF'
  chmod +x /root/Desktop/reset_microtik_switch.sh
  /root/Desktop/reset_microtik_switch.sh
EOF
}

function setup_tux_42() {
echo "Performing tux42 setup."
sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.72 << 'EOF'
  ifconfig eth1 172.16.41.1/24
  route add -net 172.16.40.0/24 gw 172.16.41.253
EOF
echo "Tux42 setup successfully."
}

function setup_tux_43() {
echo "Performing tux43 setup."
sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.73 << 'EOF'
  systemctl restart networking
  ifconfig eth1 172.16.40.1/24
  route add -net 172.16.41.0/24 gw 172.16.40.254
EOF
echo "Tux43 setup successfully."
}

function setup_tux_44() {	
echo "Performing tux44 setup."
sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.74 << 'EOF'
  systemctl restart networking
  ifconfig eth1 172.16.40.254/24
  ifconfig eth2 172.16.41.253/24
  sysctl net.ipv4.ip_forward=1
  sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
EOF
echo "Tux44 setup successfully."
}

function setup_tuxes() {
setup_tux_44 # First configure the router.
setup_tux_42
setup_tux_43
}

function reset_microtik_router() {

sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.72 << 'EOF'
  route add default gw 172.16.41.254
EOF

sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.73 << 'EOF'
  route add default gw 172.16.40.254
EOF

sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.74 << 'EOF'
  route add default gw 172.16.41.254
EOF

sshpass -p "alanturing" scp "$1" $user@10.227.20.73:/root/Desktop
sshpass -p "alanturing" ssh -o StrictHostKeyChecking=no $user@10.227.20.73 << 'EOF'
  chmod +x /root/Desktop/reset_microtik_router.sh
  /root/Desktop/reset_microtik_router.sh
EOF

}

clear

if [ -e "$1" ]; then

	if [ ! -e "$2" ]; then
    		echo "Please specify the path for router reset file in the second argument."
	fi

	echo "Performing tux setup."
    	setup_tuxes
	echo "Tux setup successfully."
	
	echo "Performing microtik switch reset."
	reset_microtik_switch "$1"
	echo "Microtik switch successfully reset."

	read -p "Configure Microtik Router? (y/N) " userOption
	
	userOption=${userOption,,}
	
	if [[ "$userOption" == "y" ]]; then
		echo "Performing microtik router reset."
		reset_microtik_router "$2"
		echo "Microtik router successfully reset."		
	fi

else
	echo "Please specify the path for switch reset file in the first argument."
fi

function reset_microtik_router_inner()
{
stty -F /dev/ttyS0 115200 cs8 -cstopb -parenb
echo -e "/system reset-configuration\r\n" > /dev/ttyS0
echo -e "y\r\n" > /dev/ttyS0

sleep 60 # change ?

echo -e "admin\r\n" > /dev/ttyS0
sleep 1

echo -e "\r\n" > /dev/ttyS0
sleep 1

echo -e "\r\n" > /dev/ttyS0
sleep 1

echo -e "/ip address add address=10.227.20.49/24 interface=ether1\r\n" > /dev/ttyS0
sleep 1

echo -e "/ip address add address=172.16.41.254/24 interface=ether2\r\n" > /dev/ttyS0
sleep 1

echo -e "/ip route add dst-address=172.16.40.0/24 gateway=172.16.41.253\r\n" > /dev/ttyS0
sleep 1

echo -e "/ip route add dst-address=0.0.0.0/0 gateway=172.16.1.41\r\n" > /dev/ttyS0
sleep 1
}

function reset_microtik_switch_inner()
{
stty -F /dev/ttyS0 115200 cs8 -cstopb -parenb
echo -e "/system reset-configuration\r\n" > /dev/ttyS0
echo -e "y\r\n" > /dev/ttyS0

sleep 60

echo -e "admin\r\n" > /dev/ttyS0
sleep 1

echo -e "\r\n" > /dev/ttyS0
sleep 1

echo -e "\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port remove [find interface=ether1]\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port remove [find interface=ether2]\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port remove [find interface=ether9]\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port remove [find interface=ether10]\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port remove [find interface=ether11]\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge add name=bridge40\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge add name=bridge41\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port add bridge=bridge40 interface=ether1\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port add bridge=bridge40 interface=ether2\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port add bridge=bridge41 interface=ether9\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port add bridge=bridge41 interface=ether10\r\n" > /dev/ttyS0
sleep 1

echo -e "/interface bridge port add bridge=bridge41 interface=ether11\r\n" > /dev/ttyS0
sleep 1
}

