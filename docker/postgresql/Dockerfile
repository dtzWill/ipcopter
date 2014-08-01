# Build PostgreSQL container with our ipcopter tech installed
# http://docs.docker.com/reference/builder/
FROM      tutum/postgresql
MAINTAINER Will Dietz <w@wdtz.org>

# Misc useful utilities
RUN apt-get install -y strace ltrace psmisc vim
# RUN apt-get install -y binutils build-essential git cmake
# RUN apt-get install -y gdb valgrind

# Grab package containing pgbench
RUN apt-get install -y postgresql-contrib

# Initialize with our password
ENV POSTGRES_PASS llvm
# RUN service postgresql start && ./modify_postgres_pass.sh && service postgresql stop

# Initialize the benchmark, using bench2 (r/w test) parameters given here:
# https://wiki.postgresql.org/wiki/Pgbenchtesting
# RUN service postgresql start && sudo -u postgres pgbench -i -s 70 && service postgresql stop


# Ensure we can write to /tmp as any user, we log there.
RUN chmod 777 /tmp

# By default, start the server and spawn a shell as the postgres user.
CMD service postgresql start && ./modify_postgres_pass.sh && (sudo -u postgres /bin/bash; service postgresql stop)


# Inject latest versions of the library and daemon:
ADD tmp/ipcd /bin/
ADD tmp/libipc.so /lib/

# Ask dynamic linker to preload libipc.so into everything!
RUN chmod 555 /bin/ipcd /lib/libipc.so
RUN echo "/lib/libipc.so" >> /etc/ld.so.preload

