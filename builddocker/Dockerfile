FROM centos:centos7.9.2009
# 设置作者信息（可选）
LABEL maintainer="yourname@example.com"

# 设置环境变量（可选）
ENV TZ=Asia/Shanghai

ARG HTTP_PROXY
ARG HTTPS_PROXY

# 更新系统并安装常用软件（示例）
RUN rm -f /etc/yum.repos.d/* \
    && curl -o /etc/yum.repos.d/CentOS-Base.repo http://mirrors.aliyun.com/repo/Centos-7.repo  \
    && yum clean all \
    && yum makecache 


RUN yum install -y vim wget libcurl* gcc make \
    && wget -i -c http://dev.mysql.com/get/mysql57-community-release-el7-11.noarch.rpm \
    && yum -y install mysql57-community-release-el7-11.noarch.rpm \
    && rpm --import https://repo.mysql.com/RPM-GPG-KEY-mysql-2022 \
    && yum -y install mysql mysql-server mysql-devel 


# 容器启动时执行的命令（示例）
CMD ["/bin/bash"]
# CMD ["mysqld", "--user=mysql", "--console"]
