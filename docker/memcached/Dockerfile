# Build memcached container with our ipcopter tech installed
# http://docs.docker.com/reference/builder/
FROM ubuntu:14.10
MAINTAINER Will Dietz <w@wdtz.org>

RUN sed -i 's@main restricted@main restricted multiverse@' /etc/apt/sources.list

# Misc useful utilities
RUN apt-get update
RUN apt-get install -y strace ltrace psmisc vim
RUN apt-get install -y binutils build-essential git cmake
RUN apt-get install -y gdb valgrind

# Install memcached
RUN apt-get install -y memcached

# And benchmark
RUN git clone https://github.com/membase/memcachetest

# More build stuff, needed for benchmark
RUN apt-get install -y autoconf

# Hack source to avoid build error.
# I believe this root is this issue:
# https://github.com/ingenthr/memcachetest/issues/9
RUN sed -i 's@HAVE_MEMCACHED_PROTOCOL_BINARY_H@__NOT_DEF__@' memcachetest/libmemc.c
# Build benchmark
RUN cd memcachetest && config/autorun.sh && ./configure && make


# # clone the benchmark repo
# RUN git clone https://github.com/antirez/mc-benchmark
# # Build it!
# RUN cd mc-benchmark && make


# Inject latest versions of the library and daemon:
ADD tmp/ipcd /bin/
ADD tmp/libipc.so /lib/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload

CMD /bin/bash
