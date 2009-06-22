#!

fgrep 3210 HardwareProfile.h | sed -e 's/\/\/.*$//' -e 's/\/\*.*\*\///' -e 's/bits//' -e 's/\./,/' > HardwareProfile.inc
