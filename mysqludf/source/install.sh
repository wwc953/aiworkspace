cd /home/mysql-udf-http-1.0
./configure --prefix=/usr/local/mysql --with-mysql=/usr/bin/mysql_config --libdir=/opt/rh/rh-mysql57/root/usr/lib64/mysql/plugin
make && make install