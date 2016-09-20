#!/bin/bash
# Copyright (C) 2015-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the client and service with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start two binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs client
# and service and checks that both exit sucessfully.

FAIL=0

# Parameter 1: the pid to check
check_tcp_udp_sockets_are_open ()
{
    # Check that the service does listen on at least one TCP/UDP socket 
    # awk is used to avoid the case when a inode number is the same as a PID. The awk
    # program filters the netstat output down to the protocol (1st field) and 
    # the PID/Program name (last field) fields.
    SERVICE_SOCKETS_LISTENING=$(netstat -tulpen 2> /dev/null | awk '{print $1 "\t"  $NF}' | grep $1 | wc -l)
    if [ $SERVICE_SOCKETS_LISTENING -lt 1 ]
    then
        ((FAIL+=1))
    fi
}

# Parameter 1: the pid to check
check_tcp_udp_sockets_are_closed ()
{
    # Check that the service does not listen on any TCP/UDP socket 
    # or has any active connection via a TCP/UDP socket
    # awk is used to avoid the case when a inode number is the same as a PID. The awk
    # program filters the netstat output down to the protocol (1st field) and 
    # the PID/Program name (last field) fields.
    SERVICE_SOCKETS_LISTENING=$(netstat -tulpen 2> /dev/null | awk '{print $1 "\t"  $NF}' | grep $1 | wc -l)
    if [ $SERVICE_SOCKETS_LISTENING -ne 0 ]
    then
        ((FAIL+=1))
    fi

    SERVICE_SOCKETS_CONNECTED=$(netstat -tupen 2> /dev/null | awk '{print $1 "\t"  $NF}' | grep $1 | wc -l)
    if [ $SERVICE_SOCKETS_CONNECTED -ne 0 ]
    then
        ((FAIL+=1))
    fi
}

# Start the service
export VSOMEIP_APPLICATION_NAME=external_local_routing_test_service
export VSOMEIP_CONFIGURATION=external_local_routing_test_service.json
./external_local_routing_test_service &
SERIVCE_PID=$!
sleep 1;

check_tcp_udp_sockets_are_open $SERIVCE_PID

# Start the client (we reuse the one from the local_routing_test to check
# the local routing functionality).
export VSOMEIP_APPLICATION_NAME=local_routing_test_client
export VSOMEIP_CONFIGURATION=local_routing_test_client.json
./local_routing_test_client &
CLIENT_PID=$!

check_tcp_udp_sockets_are_open $SERIVCE_PID
sleep 1
check_tcp_udp_sockets_are_closed $CLIENT_PID

# wait that local client finishes
sleep 2

# Display a message to show the user that he must now call the external client
# to finish the test successfully
kill -0 $CLIENT_PID &> /dev/null
CLIENT_STILL_THERE=$?
if [ $CLIENT_STILL_THERE -ne 0 ]
then
cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** external_local_routing_test_client_external_start.sh
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** external_local_routing_test_client_external.json and
** external_local_routing_test_service.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
fi

# Wait until client and service are finished
for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || ((FAIL+=1))
done

# Check if client and server both exited sucessfully and the service didnt't
# have any open 
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
