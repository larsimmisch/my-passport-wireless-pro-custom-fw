#!/bin/bash
#

timestamp=$(date "+%Y.%m.%d-%H.%M.%S")
echo $timestamp ": storage_transfer_device_available.sh called:" $@ >> /tmp/backup.log

storageType="${1}"
if [ "${storageType}" == "storageType" ]; then
	sharename="$2"
else
	sharename="$1"
fi

USBmount="/media/USB"

if [ $# == 0 ]; then
	echo "storage_transfer_device_available.sh <share_name>"
	exit 1
fi

sd_files=$(ls /tmp/mmcblk0*-info | wc -l)
if [ "${sd_files}" != 0 ]; then
	SDmount=`cat /tmp/mmcblk0*-info | grep "${sharename}" | awk -F: '{print $3}'`
	if [ "$SDmount" != "" ]; then
		sdmountpoint=`cat /proc/mounts | grep -rsw "${SDmount}" | awk '{print $1}' | awk -F\/ '{print $3}'`
		sdinfofile=/tmp/${sdmountpoint}-info
		sharemount=`cat ${sdinfofile} | grep -rsw "${sharename}"`
		if [ "$sharemount" != "" ]; then
			if [ "${storageType}" == "storageType" ]; then
				echo "SD"
			else
				echo "true"
			fi
			exit 0
		fi 
	fi
else
	echo $timestamp ": storage_transfer_device_available.sh No sd_files" >> /tmp/backup.log
fi
	
	
	
#USBmount=`df | grep /media/USB`
if [ -f "/tmp/detectInterface" ]; then
	devicefound=`cat /tmp/detectInterface`
fi
if [ "${devicefound}" == "MTP" -o "${devicefound}" == "PTP" ]; then
	sharemount=`cat /proc/mounts | grep "${USBmount}" | awk '{print $1}'`
	if [ "$sharemount" == "gphotofs" -o "$sharemount" == "simple-mtpfs" -o "$sharemount" == "ifuse" ]; then
		if [ "${storageType}" == "storageType" ]; then
			echo "USB"
		else
			echo "true"
		fi
		exit 0
	fi
else		
	files=$(ls /tmp/sd*-info | wc -l)
	if [ "${files}" != 0 ]; then
	#if [ -f /tmp/sd*-info ]; then
		USBmount=`cat /tmp/sd*-info | grep "${sharename}" | awk -F: '{print $3}'`
		if [ "$USBmount" != "" ]; then
			mountpoint=`cat /proc/mounts | grep -rsi "${USBmount}" | awk '{print $1}' | awk -F\/ '{print $3}'`
			usbinfofile=/tmp/${mountpoint}-info
			sharemount=`cat "${usbinfofile}" | grep -rsw "${sharename}"`
			if [ "$sharemount" != "" ]; then
				if [ "${storageType}" == "storageType" ]; then
					echo "USB"
				else
					echo "true"
				fi
				exit 0
			fi
		fi
	else
		echo $timestamp ": storage_transfer_device_available.sh No usb files" >> /tmp/backup.log
	fi
fi
	
echo "false" 


exit 0

