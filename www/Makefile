# Manufactures HTML file from XML file. Note: do NOT edit the HTML
# file, only the XML file.

all: smartmontools_scsi.html badblockhowto.html

smartmontools_scsi.html: smartmontools_scsi.xml 
	xmlto html-nochunks smartmontools_scsi.xml

badblockhowto.html: badblockhowto.xml 
	xmlto html-nochunks badblockhowto.xml

upload: smartmontools_scsi.html index.html
	scp smartmontools_scsi.html ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs
	scp badblockhowto.html ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs
	scp 3w-xxxx.txt ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs
	scp index.html ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs
	scp examples/*.html ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs/examples
	scp examples/*.txt ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs/examples
	scp BadBlockHowTo.txt ballen4705@smartmontools.sourceforge.net:/home/groups/s/sm/smartmontools/htdocs

clean:
	rm smartmontools_scsi.html badblockhowto.html
