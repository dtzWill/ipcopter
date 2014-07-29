# Container for testing zeromq
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

# Download zeromq
RUN apt-get install -y wget
RUN wget "http://download.zeromq.org/zeromq-4.0.4.tar.gz"

RUN tar xvf zeromq-4.0.4.tar.gz && cd zeromq-4.0.4 && ./configure && make

# Inject latest versions of the library and daemon:
ADD tmp/ipcd /bin/
ADD tmp/libipc.so /lib/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload
