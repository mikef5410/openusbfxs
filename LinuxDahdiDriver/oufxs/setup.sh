#!/bin/sh

modprobe crc-ccitt

#modprobe dahdi
#modprobe dahdi_dummy
rmmod  dahdi_dummy
rmmod  -s oufxs
rmmod  -s dahdi
insmod ../dahdi.ko debug=1
modprobe dahdi_dummy

#rmmod -s oufxs
insmod ./oufxs.ko debuglevel=3 $@
echo -n waiting for card to initialize:
sleep 1
for i in 1 2 3 4 5 6 7 8 9; do
  x=`dahdi_scan | tail -13 | fgrep description=`
  if echo $x | fgrep up-and-running > /dev/null; then
    dahdi_cfg
    echo " ok!"
#    echo "running fxstest ring"
#    #fxstest /dev/dahdi/1 ring
#    ../../../../dahdi-tools-2.2.0/fxstest /dev/dahdi/1 regdump > /tmp/lala
#    ../../../../dahdi-tools-2.2.0/fxstest /dev/dahdi/1 ring
    exit
  fi
  echo -n " $i"
  sleep 1
done
echo " no response"
echo card not found, not ready or other error\; please plug card to start
