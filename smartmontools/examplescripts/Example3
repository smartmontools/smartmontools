#! /bin/sh
#
# This is a script from the smartmontools examplescripts/ directory.
# It can be used as an argument to the -M exec Directive in
# /etc/smartd.conf, in a line like 
# -m <nomailer> -M exec /path/to/this/file
#
# Please see man 8 smartd or man 5 smartd.conf for further
# information.
#
# $Id: Example3 3958 2014-07-18 19:13:32Z chrfranke $

# Warn all users of a problem     
wall <<EOF
Problem detected with disk: $SMARTD_DEVICESTRING
Warning message from smartd is: $SMARTD_MESSAGE
Shutting down machine in 30 seconds...
EOF

# Wait half a minute
sleep 30 

# Power down the machine (uncomment the shutdown command if you really
# want to do this!)

# /sbin/shutdown -hf now

