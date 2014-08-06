# Build lmbench container with our ipcopter tech installed
# http://docs.docker.com/reference/builder/
FROM ubuntu:14.04
MAINTAINER Will Dietz <w@wdtz.org>

RUN sed -i 's@main restricted@main restricted multiverse@' /etc/apt/sources.list

# Misc useful utilities
RUN apt-get update
RUN apt-get install -y strace ltrace psmisc vim
RUN apt-get install -y binutils build-essential git cmake
RUN apt-get install -y gdb valgrind

# Install lmbench
RUN apt-get install -y lmbench

RUN apt-get install -y man screen


CMD /bin/bash

# Add config for lmbench
ADD CONFIG /

# Inject latest versions of the library and daemon:
ADD tmp/ipcd /bin/
ADD tmp/libipc.so /lib/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload
