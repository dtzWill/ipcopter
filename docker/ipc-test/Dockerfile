# Build ipcopter-ready container for development testing.
# http://docs.docker.com/reference/builder/
FROM      ubuntu
MAINTAINER Will Dietz <w@wdtz.org>

RUN sed -i 's@main restricted@main restricted multiverse@' /etc/apt/sources.list

RUN apt-get update

# Without this, libc is not dynamically linked
# RUN apt-get install -y libc6-dev

# Misc useful utilities
RUN apt-get install -y strace psmisc vim

# TCP benchmark for testing purposes:
RUN apt-get install -y netperf

# Inject latest versions of the library and daemon:
ADD libipc.so /lib/
ADD ipcd /bin/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload
