ipcopter
========

IPC Optimization Research Implementation

Contents:

ipcd/
  Trusted central manager daemon.

libipc/
  Shared library linked into applications (LD_PRELOAD)
  that hooks network functionality so that communication
  with local processes may be optimized.

docker/
  Scripts for building and injecting ipcopter into a testing container.
  See README for more information.

