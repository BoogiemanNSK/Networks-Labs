### Setting up an internal network:
1) Create 3 VMs (client, router and server).
2) Client and server should have different network adapters, set up for internal network. Router should have two adapters one to client, one to server.
3) After setting up VMs, adding strings from those txt's to /etc/network/interfaces will set up the interfaces.
4) Finally, enable forwarding for router by decommenting net.ipv4.ip_forward=1 in /etc/sysctl.conf