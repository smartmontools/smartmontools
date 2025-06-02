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
# $Id: Example1 3958 2014-07-18 19:13:32Z chrfranke $

# Save standard input into a temp file
cat > /root/tempfile

# Echo command line arguments into temp file
echo "Command line argument 1:" >> /root/tempfile
echo $1 >> /root/tempfile
echo "Command line argument 2:" >> /root/tempfile
echo $2 >> /root/tempfile
echo "Command line argument 3:" >> /root/tempfile
echo $3 >> /root/tempfile

# Echo environment variables into a temp file
echo "Variables are":       >> /root/tempfile
echo "$SMARTD_DEVICE"       >> /root/tempfile
echo "$SMARTD_DEVICESTRING" >> /root/tempfile
echo "$SMARTD_DEVICETYPE"   >> /root/tempfile
echo "$SMARTD_MESSAGE"      >> /root/tempfile
echo "$SMARTD_FULLMESSAGE"  >> /root/tempfile
echo "$SMARTD_ADDRESS"      >> /root/tempfile
echo "$SMARTD_SUBJECT"      >> /root/tempfile
echo "$SMARTD_TFIRST"       >> /root/tempfile
echo "$SMARTD_TFIRSTEPOCH"  >> /root/tempfile

# Run smartctl -a and save output in temp file
/usr/sbin/smartctl -a -d $SMARTD_DEVICETYPE $SMARTD_DEVICE >> /root/tempfile

# Email the contents of the temp file.  Solaris and
# other OSes may need to use /usr/bin/mailx below.
/usr/bin/mail -s "SMART errors detected on host: `hostname`" $SMARTD_ADDRESS < /root/tempfile

# And exit
exit 0
