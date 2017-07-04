#! /bin/bash

export RTE_SDK=/root/dpdk-17.05
export RTE_TARGET=x86_64-native-linuxapp-gcc
HUGEPGSZ=`cat /proc/meminfo  | grep Hugepagesize | cut -d : -f 2 | tr -d ' '`

echo " "
echo "========================Building========================"
echo " "
make install T=x86_64-native-linuxapp-gcc
if [ ! -f $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko ];then
    echo "## ERROR: Target does not have the DPDK UIO Kernel Module."
    echo "       To fix, please try to rebuild target."
    return
fi

echo "Unloading any existing DPDK UIO module"
/sbin/lsmod | grep -s igb_uio > /dev/null
if [ $? -eq 0 ] ; then
    sudo /sbin/rmmod igb_uio
fi

/sbin/lsmod | grep -s uio > /dev/null
if [ $? -ne 0 ] ; then
    modinfo uio > /dev/null
    if [ $? -eq 0 ]; then
	echo "Loading uio module"
	sudo /sbin/modprobe uio
    fi
fi

# UIO may be compiled into kernel, so it may not be an error if it can't
# be loaded.
echo " "
echo "========================Module UIO DPDK========================"
echo " "
echo "Loading DPDK UIO module"
sudo /sbin/insmod $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko
if [ $? -ne 0 ] ; then
    echo "## ERROR: Could not load kmod/igb_uio.ko."
    quit
fi
echo " "
echo "========================Binding Network Interfaces========================"
echo " "
if [ -d /sys/module/igb_uio ]; then
    echo "Binding networking devices to IGB UIO driver: "
    sudo ${RTE_SDK}/usertools/dpdk-devbind.py -b igb_uio 0000:00:03.0 && echo "------> PCI 0000:00:03.0 OK"
#    sudo ${RTE_SDK}/usertools/dpdk-devbind.py -b igb_uio 0000:00:08.0 && echo "------> PCI 0000:00:08.0 OK"
else
    echo "# Please load the 'igb_uio' kernel module before querying or "
    echo "# adjusting device bindings"
fi
echo " "
echo "========================Creating Hugepages========================"
echo " "
echo > .echo_tmp
for d in /sys/devices/system/node/node? ; do
    echo "echo 0 > $d/hugepages/hugepages-${HUGEPGSZ}/nr_hugepages" >> .echo_tmp
done
echo "Removing currently reserved hugepages"
sudo sh .echo_tmp
rm -f .echo_tmp

echo "Unmounting /mnt/huge and removing directory"
grep -s '/mnt/huge' /proc/mounts > /dev/null
if [ $? -eq 0 ] ; then
    sudo umount /mnt/huge
fi

if [ -d /mnt/huge ] ; then
    sudo rm -R /mnt/huge
fi

echo ""
echo "  Input the number of ${HUGEPGSZ} hugepages for each node"
echo "  Example: to have 128MB of hugepages available per node in a 2MB huge page system,"
echo "  enter '64' to reserve 64 * 2MB pages on each node"

echo > .echo_tmp
for d in /sys/devices/system/node/node? ; do
    node=$(basename $d)
    echo -n "Number of pages for $node = 128 "
    echo "echo 128 > $d/hugepages/hugepages-${HUGEPGSZ}/nr_hugepages" >> .echo_tmp
done
echo "Reserving hugepages"
sudo sh .echo_tmp
rm -f .echo_tmp

echo "Creating /mnt/huge and mounting as hugetlbfs"
sudo mkdir -p /mnt/huge

grep -s '/mnt/huge' /proc/mounts > /dev/null
if [ $? -ne 0 ] ; then
    sudo mount -t hugetlbfs nodev /mnt/huge
fi

echo " "
echo "========================DONE========================"
echo " "
