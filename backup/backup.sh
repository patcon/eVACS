#! /bin/sh

# DDSv1D-3.2: End of Day Backup
# Script to set up backup polling place electronic votes.

DB_NAME="evacs"

MASTER_DIRECTORY=/var/lib/pgsql/data
MASTER_PORT=5432

SLAVE_DIRECTORY=/var/lib/pgsql-slave/data
SLAVE_PORT=5433

WEB_IMAGES_DIRECTORY=/var/www/html/images
ZIP_DEVICE=/dev/sda4
ZIP_DIRECTORY=/mnt/zip

# We need vfat for long names.
MOUNT_OPTIONS="-t vfat"

get_vote_count()
{
    echo "SELECT COUNT(*) FROM confirmed_vote;" |
	su - postgres -c "psql -p $PORT evacs" > $1 2>&1

    if grep -q 'ERROR' $1; then
	echo -1
    else
       	tail -3 $1 | head -1 | sed 's/ //'
    fi
}

# DDSv1D-3.2: Display Backup Prompt
display_prompt()
{
    TMPFILE=`mktemp /tmp/backup.XXXXXX`
    VOTES=`get_vote_count $TMPFILE`
    if [ $VOTES -lt 0 ]; then
	echo Database error.
	cat $TMPFILE
	rm $TMPFILE
	exit 1
    fi
    rm $TMPFILE
    echo $VOTES "votes in the" $TYPE "database."
    echo -n "Insert removable media for $TYPE and press return:"
    read DISCARD
}

# DDSv1D-3.2: Check Media
check_media()
{
    # Mount the zip drive
    mount $MOUNT_OPTIONS $ZIP_DEVICE $ZIP_DIRECTORY
}

eject_media()
{
    umount $ZIP_DEVICE
    # SCSI layer gives nasty-looking (harmless) messages, so suppress them.
    dmesg -n1
    eject $ZIP_DEVICE
    dmesg -n5
}

PASS=1
# DDSv1D-3.2: Get Required Number
while [ $PASS -le 2 ] ; do

        if [ $PASS = 1 ]; then
		DIRECTORY=$MASTER_DIRECTORY
		PORT=$MASTER_PORT
		TYPE="master"
    	else
		DIRECTORY=$SLAVE_DIRECTORY
		PORT=$SLAVE_PORT
		TYPE="slave"
    	fi

    	display_prompt
    	check_media

    	# Now export confirmed_vote table onto the zip disk

    	# DDSv1D-3.2: Backup Confirmed Votes
    	# DDSv1D-3.2: Get Confirmed Vote
    	# DDSv1D-3.2: Write Confirmed Vote
	su - postgres -c "pg_dump $DB_NAME -p $PORT -c -t confirmed_vote && pg_dump $DB_NAME -p $PORT -c -t confirmed_preference" > $ZIP_DIRECTORY/$TYPE.dmp

    	if [ $? != 0 ]; then
		# DDSv1D-3.2: Format Backup Error Message
		echo Backup of $TYPE database failed!
		exit 1
    	fi

    	#Increment pass count
    	# DDSv1D-3.2: Check & Update Count
    	# DDSv1D-3.2: Update Count of Backups

    	let PASS=$PASS+1

	# Display the sum of the output file, plus random garbage so votes
	# not derivable from it.
	od -x -N128 < /dev/urandom > $ZIP_DIRECTORY/$TYPE.rnd
	echo The ballot contents number for ${TYPE}: `cat $ZIP_DIRECTORY/$TYPE.dmp $ZIP_DIRECTORY/$TYPE.rnd | md5sum`

    	eject_media
done

echo
echo Thankyou.  End of Day Backup complete.
exit 0
