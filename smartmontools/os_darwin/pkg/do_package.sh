#!/bin/sh

VERSION=6.5 # fix me, take from svn

mkdir -p pkg
# creating payload
( cd root && find . | cpio -o --format odc --owner 0:80 | gzip -c ) > pkg/Payload
# packing scripgs
( cd scripts && find . | cpio -o --format odc --owner 0:80 | gzip -c ) > pkg/Scripts
#  number of files and folders inside the payload
PAYLOAD_FILES=`find root | wc -l`
PAYLOAD_SIZEKB=`du -BK  -s root|awk '{print $1}'|tr -d 'K'`
cp PackageInfo.in pkg/PackageInfo
sed -i -e "s|@version@|$VERSION|" -e "s|@files@|${PAYLOAD_FILES}|" -e "s|@size@|${PAYLOAD_SIZEKB}|"  pkg/PackageInfo
cp Distribution.in pkg/Distribution
sed -i -e "s|@version@|$VERSION|" -e "s|@files@|${PAYLOAD_FILES}|" -e "s|@size@|${PAYLOAD_SIZEKB}|"  pkg/Distribution
# add license
mkdir -p pkg/Resources/English.lproj
cp ../COPYING pkg/Resources/English.lproj/license.txt
mkbom -u 0 -g 80 root pkg/Bom
###

# build package
mkdir -p installer
( cd pkg && xar --compression none -cf "../installer/smartmontools.pkg" * )
# make an iso 	${MKISOFS} -V 'DMDirc' -no-pad -r -apple -map ${MAPFILE} -o "${OUTPUTFILE}" -hfs-bless "${BUILDDIR}" "${BUILDDIR}"
mkisofs -V 'smartmontools' -no-pad -r -apple -o smartmontools.iso -hfs-bless "installer/" "installer/"
# pack resulted dmg image
dmg dmg smartmontools.iso smartmontools_${VERSION}.dmg
