#!/bin/bash

. ./build.sh

TEST_CMD="
java -jar dacapo.jar -size large \
	avrora batik eclipse fop h2 jython \
	luindex lusearch pmd sunflow tomcat \
	tradebeans tradesoap xalan
"

. ../test.inc


