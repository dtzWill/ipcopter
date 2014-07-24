# Build PostgreSQL container with our ipcopter tech installed
# http://docs.docker.com/reference/builder/
FROM      tutum/postgresql
MAINTAINER Will Dietz <w@wdtz.org>

# Misc useful utilities
RUN apt-get install -y strace ltrace psmisc vim
# RUN apt-get install -y binutils build-essential git cmake
# RUN apt-get install -y gdb valgrind


# Inject latest versions of the library and daemon:
ADD tmp/libipc.so /lib/
ADD tmp/ipcd /bin/

# Ask dynamic linker to preload libipc.so into everything!
RUN chmod 555 /bin/ipcd /lib/libipc.so
RUN chmod 777 /tmp
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload

CMD /bin/bash
