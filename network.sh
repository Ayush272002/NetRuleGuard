#!/bin/bash

# Globals
ret=0
server="./server"
client="./client"
serverOut="testServerOutput.txt"
clientOut="testClientOutput.txt"
successFile="testSuccess.txt"
debugFile="testDebug.txt"
valgrindLog="valgrind_log.txt"
IPADDRESS=localhost
PORT=2200

# --- helper functions ---
function check_result() {
    local expected="$1"
    local test_name="$2"

    echo -en "$test_name: \t"
    echo "=== Test: $test_name ===" >> $debugFile
    echo "Expected output:" >> $debugFile
    echo -e "$expected\n" >> $debugFile
    echo "Actual output:" >> $debugFile
    cat $clientOut >> $debugFile
    echo -e "\n-------------------\n" >> $debugFile

    sed -i 's/[[:space:]]*$//' $clientOut
    echo -e "$expected" > $successFile
    sed -i 's/[[:space:]]*$//' $successFile

    res=$(diff $clientOut $successFile)
    if [ "$res" != "" ]; then
        echo "Error: Client returned invalid result (see $debugFile for details)"
        return 1
    else
        echo "OK"
        return 0
    fi
}

# --- Valgrind Memory Check ---
function memory_check() {
    echo "Running Valgrind memory check..."
    valgrind --leak-check=full --log-file=$valgrindLog $server $PORT > $serverOut 2>&1 &
    sleep 1  # Allow time for the server to start up

    if grep -q "ERROR SUMMARY: 0 errors" $valgrindLog; then
        echo "No memory leaks detected."
    else
        echo "Memory leaks detected! See $valgrindLog for details."
        ret=1
    fi
}

# --- Test Cases ---

function test_basic_rules() {
    echo "Testing basic rule operations in network mode..."

    # Add a single rule
    rm -f $clientOut $successFile
    echo "Rule added" > $successFile
    ./$client $IPADDRESS $PORT "A 147.188.193.15 22" > $clientOut 2>&1
    check_result "Rule added" "Adding single rule via network"

    # Memory check
    memory_check
}

function test_edge_cases() {
    echo "Testing edge cases in network mode..."

    # Boundary IP test
    rm -f $clientOut $successFile
    echo -e "Rule added\nRule added\nConnection accepted\nConnection accepted" > $successFile
    ./$client $IPADDRESS $PORT "A 0.0.0.0 22" > $clientOut 2>&1
    ./$client $IPADDRESS $PORT "A 255.255.255.255 22" >> $clientOut 2>&1
    ./$client $IPADDRESS $PORT "C 0.0.0.0 22" >> $clientOut 2>&1
    ./$client $IPADDRESS $PORT "C 255.255.255.255 22" >> $clientOut 2>&1
    check_result "Rule added\nRule added\nConnection accepted\nConnection accepted" "Boundary IP test in network mode"

    # Overlapping rule ranges
    rm -f $clientOut $successFile
    echo -e "Rule added\nRule added\nConnection accepted" > $successFile
    ./$client $IPADDRESS $PORT "A 192.168.1.0-192.168.1.255 80-90" > $clientOut 2>&1
    ./$client $IPADDRESS $PORT "A 192.168.1.128-192.168.1.200 85" >> $clientOut 2>&1
    ./$client $IPADDRESS $PORT "C 192.168.1.150 85" >> $clientOut 2>&1
    check_result "Rule added\nRule added\nConnection accepted" "Overlapping rule ranges in network mode"

    # Memory check
    memory_check
}

function test_invalid_commands() {
    echo "Testing invalid commands in network mode..."

    # Test with an invalid command
    rm -f $clientOut $successFile
    echo "Illegal request" > $successFile
    ./$client $IPADDRESS $PORT "X 123.123.123.123 80" > $clientOut 2>&1
    check_result "Illegal request" "Testing unknown command in network mode"

    # Memory check
    memory_check
}

function test_port_boundaries() {
    echo "Testing port boundaries in network mode..."

    # Port range boundaries
    rm -f $clientOut $successFile
    echo -e "Rule added\nConnection accepted" > $successFile
    ./$client $IPADDRESS $PORT "A 192.168.1.0-192.168.1.255 0-65535" > $clientOut 2>&1
    ./$client $IPADDRESS $PORT "C 192.168.1.50 65535" >> $clientOut 2>&1
    check_result "Rule added\nConnection accepted" "Testing upper port boundary in network mode"

    # Test invalid port
    rm -f $clientOut $successFile
    echo "Illegal IP address or port specified" > $successFile
    ./$client $IPADDRESS $PORT "C 192.168.1.50 70000" > $clientOut 2>&1
    check_result "Illegal IP address or port specified" "Testing invalid port in network mode"

    # Memory check
    memory_check
}

function test_rule_deletion() {
    echo "Testing rule deletion in network mode..."

    # Deleting a rule
    rm -f $clientOut $successFile
    echo -e "Rule added\nRule deleted" > $successFile
    ./$client $IPADDRESS $PORT "A 147.188.193.15 22" > $clientOut 2>&1
    ./$client $IPADDRESS $PORT "D 147.188.193.15 22" >> $clientOut 2>&1
    check_result "Rule added\nRule deleted" "Deleting existing rule in network mode"

    # Delete non-existent rule
    rm -f $clientOut $successFile
    echo "Rule not found" > $successFile
    ./$client $IPADDRESS $PORT "D 147.188.193.16 22" > $clientOut 2>&1
    check_result "Rule not found" "Deleting non-existent rule in network mode"

    # Memory check
    memory_check
}

# --- Main test execution ---
echo "Starting Network Mode Tests..."
rm -f $debugFile $valgrindLog

# Start server
echo "Starting server on port $PORT..."
valgrind --leak-check=full --log-file=$valgrindLog $server $PORT > $serverOut 2>&1 &
sleep 1  # Wait for server to start

# Execute test cases
test_basic_rules
test_edge_cases
test_invalid_commands
test_port_boundaries
test_rule_deletion

echo "Debug information has been written to $debugFile"
echo "Memory check results in $valgrindLog"

# Summary and Cleanup
if grep -q "Memory leaks detected" $valgrindLog; then
    echo "Some memory issues detected - check $valgrindLog for details."
fi

if [ $ret != 0 ]; then
    echo "Some tests failed - check $debugFile for details"
else
    echo "All tests passed successfully"
fi

# Kill the server
killall server

exit $ret
