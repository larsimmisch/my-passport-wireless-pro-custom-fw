#!/bin/bash
#
# � 2010 Western Digital Technologies, Inc. All rights reserved.
#
# getFixedNtpServer.sh
#
#

#---------------------



PATH=/sbin:/bin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
. /etc/nas/config/share-param.conf
. /etc/system.conf


#SYSTEM_SCRIPTS_LOG=${SYSTEM_SCRIPTS_LOG:-"/dev/null"}
## Output script log start info
#{ 
#echo "Start: `basename $0` `date`"
#echo "Param: $@" 
#} >> ${SYSTEM_SCRIPTS_LOG}
##
#{
#---------------------
# Begin Script
#---------------------

awk '
BEGIN { 
    RS = "\n" ; FS = "=";
}
{
	if ( $1 == "NTPSERVERS" ) {
		# remove double quotes
		gsub("\"","",$2);
		split($2, list, " ");
		for (i in list) n++;
		if (n == 3) {
			print list[2];
			print list[3];
		}
		else {
			print list[1];
			print list[2];
		}
	}
}
' ${ntpConfig}

#---------------------
# End Script
#---------------------
## Copy stdout to script log also
#} # | tee -a ${SYSTEM_SCRIPTS_LOG}
## Output script log end info
#{ 
#echo "End:$?: `basename $0` `date`" 
#echo ""
#} >> ${SYSTEM_SCRIPTS_LOG}