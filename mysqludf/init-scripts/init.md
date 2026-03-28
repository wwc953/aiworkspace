-- root用户进入
docker exec -it --user root 5df64c72f2aaa743ec0346d57f3e3634f1dec2d8860c0f1fbeb455a17edd47a9  bash

#删除原数据源
rm -f /etc/yum.repos.d/*
curl -o /etc/yum.repos.d/CentOS-Base.repo http://mirrors.aliyun.com/repo/Centos-7.repo
yum clean all
yum makecache
yum install -y libcurl* gcc mysql-devel make
tar -zxvf /home/mysql-udf-http-1.0.tar.gz
cd /home/mysql-udf-http-1.0
./configure --prefix=/usr/local/mysql --with-mysql=/usr/bin/mysql_config --libdir=/opt/rh/rh-mysql57/root/usr/lib64/mysql/plugin
make && make install


mysql -h 127.0.0.1 -P 3306 -u root -p'rootpassword' < /home/setup-udf.sql

