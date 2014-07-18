#! /bin/sh
#
# This is a script from the smartmontools examplescripts/ directory.
# It can be used as an argument to the -M exec Directive in
# /etc/smartd.conf, in a line like 
# -m root@localhost -M exec /path/to/this/file
#
# Please see man 8 smartd or man 5 smartd.conf for further
# information.
#
# $Id: Example2 3958 2014-07-18 19:13:32Z chrfranke $

# Save the email message (STDIN) to a file:
cat > /root/msg

# Append the output of smartctl -a to the message:
/usr/sbin/smartctl -a -d $SMARTD_DEVICETYPE $SMARTD_DEVICE >> /root/msg

# Now email the message to the user.  Solaris and
# other OSes may need to use /usr/bin/mailx below.
/usr/bin/mail -s "$SMARTD_SUBJECT" $SMARTD_ADDRESS < /root/msg

