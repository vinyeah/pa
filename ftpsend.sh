#!/bin/bash

TMP_PA_DIR=/tmp/.pa_ftp_shell
mkdir -p $TMP_PA_DIR

LOCAL_FILE=$1
LOG_TYPE=$2
DIR_DATE=$3
DIR_HOUR=$4
DIR_MIN=$5
DEST_NAME=$6

rm -f $TMP_PA_DIR/* >/dev/null 2>&1 
cp $LOCAL_FILE $TMP_PA_DIR/$DEST_NAME
touch $TMP_PA_DIR/${DEST_NAME}.ok



ftp -n<<!
open log.bhunetworks.com
user ftpdlog ftpBhu8273
bin
passive
prompt
mkdir $LOG_TYPE
cd $LOG_TYPE
mkdir $DIR_DATE
cd $DIR_DATE
mkdir $DIR_HOUR
cd $DIR_HOUR
mkdir $DIR_MIN
cd $DIR_MIN
lcd $TMP_PA_DIR
mput $DEST_NAME
mput $DEST_NAME.ok
close
bye
!



mkdir -p ./done2
cp $1 ./done2/$6

