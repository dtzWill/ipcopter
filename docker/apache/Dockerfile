# Container for testing apache
FROM      ubuntu:14.04
MAINTAINER Will Dietz <w@wdtz.org>

RUN sed -i 's@main restricted@main restricted multiverse@' /etc/apt/sources.list

RUN apt-get update

# Without this, libc is not dynamically linked
# RUN apt-get install -y libc6-dev

# Misc useful utilities
RUN apt-get install -y strace ltrace psmisc vim
RUN apt-get install -y binutils build-essential git cmake
RUN apt-get install -y gdb valgrind

RUN apt-get install -y apache2
RUN apt-get install -y apache2-utils
RUN apt-get install -y wget curl links

# Create test files
RUN for s in 100M 10M 1M 100K 10K 1K; do dd if=/dev/urandom of=/var/www/html/$s.bin bs=$s count=1; done

# Inject latest versions of the library and daemon:
ADD tmp/ipcd /bin/
ADD tmp/libipc.so /lib/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload
