#! /bin/sh 
# Master script to set dump the EVACS database.

SETUP_DIR=/var/lib/postgres-slave/setup
ZIP_DIRECTORY=/mnt/zip

# the db name is;
DB_NAME=evacs

# the name of the db file is;
DB_FILE=ENS_dump.dmp

DB_FILE_TARRED=ENS_tar.gz

# We need vfat for long names.
# We want postgres to own all files.
MOUNT_OPTIONS="-t vfat -o uid=`id -u postgres`"

bailout()
{
    echo "$@" >&2
    exit 1
}


# Unmount in case mounted already
umount $ZIP_DIRECTORY 2>/dev/null

# Working directory, owned by postgres user
rm -rf  $SETUP_DIR
mkdir $SETUP_DIR || bailout "Could not make $SETUP_DIR"
chown postgres $SETUP_DIR


while true; do
    echo Insert zip into drive and press ENTER.
    read

    # Try to mount Zip disk.
    [ -d $ZIP_DIRECTORY ] || mkdir $ZIP_DIRECTORY
    mount $MOUNT_OPTIONS /dev/sda4 $ZIP_DIRECTORY && break
	echo "Couldn't mount Zip disk" >&2
done


# remove files on the zip drive
rm -f $ZIP_DIRECTORY/$DB_FILE
rm -f /tmp/$DB_FILE

# Change ownership to postgres user.
chown postgres $ZIP_DIRECTORY

#Dump the db to a tmp file
su - postgres -c "pg_dump $DB_NAME -c > $SETUP_DIR/$DB_FILE"
if [ $? != 0 ]; then
	echo pg_dump of $DB_NAME failed.
	rm -rf $SETUP_DIR
	umount  $ZIP_DIRECTORY
	exit 1
fi

#tar the database and put it on the zip disk
cd "$SETUP_DIR"
tar -czf $ZIP_DIRECTORY/$DB_FILE_TARRED $DB_FILE
if [ $? != 0 ]; then
	echo Tarring the database failed.
	#rm -rf $SETUP_DIR
	umount $ZIP_DIRECTORY
	exit 1
fi

cd /root/

umount  $ZIP_DIRECTORY
rm -rf $SETUP_DIR

echo Thankyou. Copying of database to zip disk complete.

exit 0







