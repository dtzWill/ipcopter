#!/bin/sh

. ./build.sh

TEST_CMD="
sed -i 's/md5/trust/g' /etc/postgresql/9.3/main/pg_hba.conf
service postgresql start
sudo -u postgres pgbench -i -s 70 -h localhost
time sudo -u postgres pgbench -s 70 -t 30000
service postgresql stop"

. ../test.inc


