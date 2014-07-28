# Build ipcopter-ready container for development testing.
# http://docs.docker.com/reference/builder/
FROM      ubuntu:14.10
MAINTAINER Will Dietz <w@wdtz.org>

RUN sed -i 's@main restricted@main restricted multiverse@' /etc/apt/sources.list

RUN apt-get update

# Without this, libc is not dynamically linked
# RUN apt-get install -y libc6-dev

# Misc useful utilities
RUN apt-get install -y strace ltrace psmisc vim
RUN apt-get install -y binutils build-essential git cmake
RUN apt-get install -y gdb valgrind

# TCP benchmark for testing purposes:
RUN apt-get install -y netperf iperf

# More software...
RUN apt-get install -y openssh-server
# Permit login with root password
RUN sed --in-place=.bak 's/without-password/yes/' /etc/ssh/sshd_config
# ssh needs this directory
RUN mkdir -p /var/run/sshd

ENV HOME /root/

# Generate key and install into authorized_keys
RUN mkdir /root/.ssh && chmod 700 /root/.ssh && ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && cp /root/.ssh/id_rsa.pub /root/.ssh/authorized_keys

# More software
RUN apt-get install -y pv netcat

# Inject latest versions of the library and daemon:
ADD start-sshd.sh /
ADD tmp/ipcd /bin/
ADD tmp/libipc.so /lib/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload

CMD /bin/bash
