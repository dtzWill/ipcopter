# Container for testing apache
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

RUN apt-get update
RUN apt-get install -y openjdk-7-jdk

# Copy java source
ADD src /src

RUN cd src && javac *.java

# Inject latest versions of the library and daemon:
ADD tmp/libipc.so /lib/
ADD tmp/ipcd /bin/

# Ask dynamic linker to preload libipc.so into everything!
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload
