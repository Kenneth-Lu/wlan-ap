#!/bin/bash

IF_LIST_PATH="/var/run/nf_ingress_ifs.conf"
NFT_OVS_OFFLOAD_PATH="/var/run/ovs_offload.conf"
NF_TBL_NAME="ovs_sw_acc_table"
NF_TBL_FAMILY="ip"
NF_FLOWTABLE_NAME="ft"

nft_flush() {
    nft delete table $NF_TBL_NAME &>/dev/null
    rm -f $NFT_OVS_OFFLOAD_PATH
    rm -f $IF_LIST_PATH
}

nft_reload() {
    nft delete table $NF_TBL_NAME &>/dev/null
    if_list_str=$(cat $IF_LIST_PATH | tr '\n' ',' | head -c -1)

    if [ -z $if_list_str ]; then
        rm -f $NFT_OVS_OFFLOAD_PATH
        return
    fi

    ########################################
    rm -f $NFT_OVS_OFFLOAD_PATH
    cat <<EOT >> $NFT_OVS_OFFLOAD_PATH
table $NF_TBL_FAMILY $NF_TBL_NAME {
    flowtable $NF_FLOWTABLE_NAME {
        hook ingress priority 0
        devices = { $if_list_str }
    }
    chain forward {
        type filter hook forward priority 0
        ip protocol tcp flow offload @$NF_FLOWTABLE_NAME
        ip protocol udp flow offload @$NF_FLOWTABLE_NAME
    }
}
EOT
    ########################################

    nft -f $NFT_OVS_OFFLOAD_PATH
}

register_hook() {
    if [ -d "/sys/class/net/$1" ]; then
        intfs=$(cat $IF_LIST_PATH)
        for intf in $intfs
        do
            if [ "$intf" == "$1" ]; then
                return 1
            fi
        done
        echo $1 >> $IF_LIST_PATH
        nft_reload
        return 0
    else
        return 1
    fi
}

unregister_hook() {
    if [ -f "$IF_LIST_PATH" ]; then
        sed -i "/$1/d" $IF_LIST_PATH
        nft_reload
    fi
}

usage() {
cat << EOF
Usage: nf_ingress_hook.sh [options]

Options:
  -c, --command     add|del|flush
  -i, --interface   interface name
  -h, --help        Show this help
EOF
}

while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -c|--command)
        COMMAND="$2"
        shift
        shift
        ;;
    -i|--interface)
        INTERFACE="$2"
        shift
        shift
        ;;
    -h|--help)
        usage
        exit 1
        ;;
    *)
        ;;
esac
done

if [ -z "$COMMAND" ]; then
    usage
    exit 1
fi

if [ "$COMMAND" = "add" ] && [ ! -z "$INTERFACE" ]; then
    register_hook $INTERFACE
    exit $?
elif [ "$COMMAND" = "del" ] && [ ! -z "$INTERFACE" ]; then
    unregister_hook $INTERFACE
    exit 0
elif [ "$COMMAND" = "flush" ]; then
    nft_flush
    exit 0
else
    usage
    exit 1
fi
