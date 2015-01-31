#!/bin/bash

# Compute LOC figures for components of ipcopter.

# Uses 'cloc' instead of 'sloccount' for Go support (ipcd).
IPCD_SOURCES="ipcd/*.go"

LIBIPC_SOURCES="libipc/*.h libipc/*.cpp"

echo "IPCD:"
cloc --quiet $IPCD_SOURCES

echo
echo
echo "LIBIPC:"
cloc --quiet $LIBIPC_SOURCES

echo
echo
echo "Together:"
cloc --quiet $IPCD_SOURCES $LIBIPC_SOURCES
