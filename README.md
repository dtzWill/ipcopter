ipcopter
========

IPC Optimization Research Implementation

Contents:

* ipcd/
  * Trusted central manager daemon.
* libipc/
  * Shared library linked into applications (LD_PRELOAD)
  * Hooks network functionality and coordinates with ipcd to optimize communication
* docker/
  * Scripts for building and injecting ipcopter into a testing container.
  * See README for more information.

