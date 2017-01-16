#!/bin/sh

useradd -r -s /bin/false www
cp gwan /etc/init.d
chmod 755 /etc/init.d
update-rc.d gwan defaults 3 6
