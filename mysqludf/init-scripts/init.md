-- root用户进入
docker exec -it --user root 1e3346fab5a32c06894b16bf7084dce8b160a1d9638a8bc3ce4a779366cbfb58  bash

#删除原数据源
rm -f /etc/yum.repos.d/*
curl -o /etc/yum.repos.d/CentOS-Base.repo http://mirrors.aliyun.com/repo/Centos-7.repo
yum clean all
yum makecache
yum install -y libcurl* gcc mysql-devel make
cd /home
tar -zxvf  mysql-udf-http-1.0.tar.gz
cp /home/mysql-udf-http_json.c /home/mysql-udf-http-1.0/src/mysql-udf-http.c
cd mysql-udf-http-1.0
./configure --prefix=/usr/local/mysql --with-mysql=/usr/bin/mysql_config --libdir=/opt/rh/rh-mysql57/root/usr/lib64/mysql/plugin
make && make install


mysql -h 127.0.0.1 -P 3306 -u root -p'rootpassword' < /home/setup-udf.sql

