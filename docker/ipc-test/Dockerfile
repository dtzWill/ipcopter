FROM      wdtz/libc-dev
MAINTAINER Will Dietz <w@wdtz.org>

RUN sed -i 's@main restricted@main restricted multiverse@' /etc/apt/sources.list

RUN apt-get update
RUN apt-get install -y strace
RUN apt-get install -y psmisc
RUN apt-get install -y netperf

ADD libipc.so /lib/
ADD ipcd /bin/

RUN echo "/lib/libipc.so" >> /etc/ld.so.preload
