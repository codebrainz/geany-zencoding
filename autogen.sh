#!/bin/bash
autoreconf -vfi || exit 1
echo "Build system prepared, now type \`./configure' to configure it."
exit 0
