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

# Change ownership to postgres user.
chown postgres $ZIP_DIRECTORY

#Dump the db to a tmp file
su - postgres -c "cp $ZIP_DIRECTORY/$DB_FILE_TARRED $SETUP_DIR/$DB_FILE_TARRED"
if [ $? != 0 ]; then
	echo  "cp $ZIP_DIRECTORY/$DB_FILE_TARRED $SETUP_DIR/$DB_FILE_TARRED failed: $?"
	rm -rf $SETUP_DIR
	umount  $ZIP_DIRECTORY
	exit 1
fi

#unttar the database
cd "$SETUP_DIR"
tar -xzf $SETUP_DIR/$DB_FILE_TARRED
if [ $? != 0 ]; then
	echo extracting tarred the database failed.
	#rm -rf $SETUP_DIR
	umount $ZIP_DIRECTORY
	exit 1
fi

su - postgres -c "psql $DB_NAME < $SETUP_DIR/$DB_FILE"

cd /root/

umount  $ZIP_DIRECTORY
rm -rf $SETUP_DIR

echo Thankyou. Restoration of database from zip disk complete.

exit 0







