Building Source
---------------

// Make sure you are in the directory containing this README file.
// This directory contains the iperf3 github repo

// Checkout a branch in the iperf3 repo using the v 3.6 tag:
#git checkout tags/3.6 -b rperf3_variable_packet_length

// For first build run bootstrap and configure.

#./bootstrap.sh

#./configure --disable-shared --without-openssl && make

// Make changes...

#make

