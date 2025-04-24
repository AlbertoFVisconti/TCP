#!/bin/bash

IP_EXEC='sudo ip'

create_host_router () {
    n="$1"
    echo "creating router R$n and host H$n"
    $IP_EXEC netns add R$n
    $IP_EXEC netns add H$n
    $IP_EXEC link add H$n-eth0 type veth peer name R$n-eth0
    $IP_EXEC link set H$n-eth0 netns H$n
    $IP_EXEC link set R$n-eth0 netns R$n

    $IP_EXEC netns exec H$n ifconfig H$n-eth0 10.0.$n.1 netmask 255.255.255.0
    $IP_EXEC netns exec R$n ifconfig R$n-eth0 10.0.$n.254 netmask 255.255.255.0
    $IP_EXEC netns exec H$n ifconfig lo up
    $IP_EXEC netns exec R$n ifconfig lo up

    $IP_EXEC netns exec H$n ip route add default via 10.0.$n.254 dev H$n-eth0

    $IP_EXEC netns exec R$n sysctl net.ipv4.ip_forward=1
}

delete_host_router () {
    n="$1"
    echo "deleting router R$n and host H$n"
    $IP_EXEC netns delete R$n
    $IP_EXEC netns delete H$n
}

link_count=1

create_link () {
    if [ $1 -gt $2 ]
    then
       u=$2
       v=$1
    else
       u=$1
       v=$2
    fi
    echo "creating link R$u -- R$v"
    $IP_EXEC link add R$u-eth$v type veth peer name R$v-eth$u # .addLink(r1,r2,intfName1='R1-eth2',...)
    $IP_EXEC link set R$u-eth$v netns R$u
    $IP_EXEC link set R$v-eth$u netns R$v
    $IP_EXEC netns exec R$u ifconfig R$u-eth$v 10.$u.$v.$u netmask 255.255.255.0
    $IP_EXEC netns exec R$v ifconfig R$v-eth$u 10.$u.$v.$v netmask 255.255.255.0

    shift
    shift
    while test -n "$1"
    do
	$IP_EXEC netns exec R$u tc qdisc add dev R$u-eth$v root $1
	$IP_EXEC netns exec R$v tc qdisc add dev R$v-eth$u root $1
	shift
    done
}

configure_link () {
    if [ $1 -gt $2 ]
    then
       u=$2
       v=$1
    else
       u=$1
       v=$2
    fi
    echo "configuring link R$u -- R$v"
    if [ -n "$1" ]
    then
	$IP_EXEC netns exec R$u tc qdisc replace dev R$u-eth$v root "$@"
	$IP_EXEC netns exec R$v tc qdisc replace dev R$v-eth$u root "$@"
    else
	$IP_EXEC netns exec R$u tc qdisc remove dev R$u-eth$v root
	$IP_EXEC netns exec R$v tc qdisc remove dev R$v-eth$u root
    fi
}

configure_link () {
    if [ $1 -gt $2 ]
    then
       u=$2
       v=$1
    else
       u=$1
       v=$2
    fi
    shift
    shift
    case "$1" in
	show )
	    echo "traffic control on link R$u -- R$v:"
	    $IP_EXEC netns exec R$u tc -p qdisc show dev R$u-eth$v
	    $IP_EXEC netns exec R$v tc qdisc show dev R$v-eth$u
	    ;;
	reset )
	    echo "removing traffic control on link R$u -- R$v"
	    $IP_EXEC netns exec R$u tc qdisc delete dev R$u-eth$v root
	    $IP_EXEC netns exec R$v tc qdisc delete dev R$v-eth$u root
	    ;;
	* )
	    echo "changing traffic control on link R$u -- R$v"
	    $IP_EXEC netns exec R$u tc qdisc replace dev R$u-eth$v root netem "$@"
	    $IP_EXEC netns exec R$v tc qdisc replace dev R$v-eth$u root netem "$@"
	    ;;
    esac
}

add_fwd_rule () {
    a=$1
    b=$2
    if [ $a -gt $b ]
    then
       u=$b
       v=$a
    else
       u=$a
       v=$b
    fi
    dst=10.0.$3.0/24
    $IP_EXEC netns exec R$a ip route add $dst via 10.$u.$v.$b dev R$a-eth$b
}

run_terminal_host () {
    echo "starting shell on host H$1"
    exec $IP_EXEC netns exec H$1 sudo -u $USER bash -c "exec bash -i <<< \"PS1='H$1> '; exec </dev/tty\""
}

run_terminal () {
    echo "starting shell in namespace $1"
    exec $IP_EXEC netns exec $1 sudo -u $USER bash -c "exec bash -i <<< \"PS1='$1> '; exec </dev/tty\""
}

case "$1" in
    off | down )
	delete_host_router 1
	delete_host_router 2
	;;
    1 | 2 )
	run_terminal_host "$1"
	;;
    H[0-9]* | R[0-9]* )
	run_terminal "$1"
	;;
    link )
	shift
	configure_link 1 2 "$@"
	;;
    "" | on | up )
	create_host_router 1
	create_host_router 2
	#
	create_link 1 2 "netem delay 50ms 30ms loss 5% rate 1mbit"
	#
	add_fwd_rule 1 2 2
	add_fwd_rule 2 1 1
	;;
esac
