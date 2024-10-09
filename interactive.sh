#!/bin/bash

# Globals
ret=0
server="./server"
serverOut="testServerOutput.txt"
successFile="testSuccess.txt"
debugFile="testDebug.txt"
cmdFile="test_commands.txt"
valgrindLog="valgrind_log.txt"

# --- helper functions ---
function check_result() {
    local expected="$1"
    local test_name="$2"

    echo -en "$test_name: \t"
    echo "=== Test: $test_name ===" >> $debugFile
    echo "Expected output:" >> $debugFile
    echo -e "$expected\n" >> $debugFile
    echo "Actual output:" >> $debugFile
    cat $serverOut >> $debugFile
    echo -e "\n-------------------\n" >> $debugFile

    sed -i 's/[[:space:]]*$//' $serverOut
    echo -e "$expected" > $successFile
    sed -i 's/[[:space:]]*$//' $successFile

    res=$(diff $serverOut $successFile)
    if [ "$res" != "" ]; then
        echo "Error: Server returned invalid result (see $debugFile for details)"
        return 1
    else
        echo "OK"
        return 0
    fi
}

# --- Valgrind Memory Check ---
function memory_check() {
    echo "Running Valgrind memory check..."
    valgrind --leak-check=full --log-file=$valgrindLog $server -i < $cmdFile > $serverOut 2>&1
    if grep -q "ERROR SUMMARY: 0 errors" $valgrindLog; then
        echo "No memory leaks detected."
    else
        echo "Memory leaks detected! See $valgrindLog for details."
        ret=1
    fi
}

# --- Test Cases ---

function test_basic_rules() {
    echo "Testing basic rule operations..."

    # Add a single rule
    rm -f $serverOut $successFile
    echo "A 147.188.193.15 22" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Rule added" "Adding single rule"

    # Memory check
    memory_check
}

function test_edge_cases() {
    echo "Testing edge cases..."

    # Boundary IP test
    rm -f $serverOut $successFile
    echo -e "A 0.0.0.0 22\nA 255.255.255.255 22\nC 0.0.0.0 22\nC 255.255.255.255 22" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Rule added\nRule added\nConnection accepted\nConnection accepted" "Testing boundary IP addresses"

    # Overlapping rule ranges
    rm -f $serverOut $successFile
    echo -e "A 192.168.1.0-192.168.1.255 80-90\nA 192.168.1.128-192.168.1.200 85\nC 192.168.1.150 85" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Rule added\nRule added\nConnection accepted" "Overlapping rule ranges"

    # Memory check
    memory_check
}

function test_invalid_commands() {
    echo "Testing invalid commands..."

    # Test with an invalid command
    rm -f $serverOut $successFile
    echo "X 123.123.123.123 80" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Illegal request" "Testing unknown command"

    # Memory check
    memory_check
}

function test_port_boundaries() {
    echo "Testing port boundaries..."

    # Port range boundaries
    rm -f $serverOut $successFile
    echo -e "A 192.168.1.0-192.168.1.255 0-65535\nC 192.168.1.50 65535" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Rule added\nConnection accepted" "Testing upper port boundary"

    # Test invalid port
    rm -f $serverOut $successFile
    echo "C 192.168.1.50 70000" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Illegal IP address or port specified" "Testing invalid port"

    # Memory check
    memory_check
}

function test_rule_deletion() {
    echo "Testing rule deletion..."

    # Deleting a rule
    rm -f $serverOut $successFile
    echo -e "A 147.188.193.15 22\nD 147.188.193.15 22" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Rule added\nRule deleted" "Deleting existing rule"

    # Delete non-existent rule
    rm -f $serverOut $successFile
    echo "D 147.188.193.16 22" > $cmdFile
    cat $cmdFile | $server -i > $serverOut 2>&1
    check_result "Rule not found" "Deleting non-existent rule"

    # Memory check
    memory_check
}

# --- Main test execution ---
echo "Starting Comprehensive Tests..."
rm -f $debugFile $valgrindLog

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

rm -f $cmdFile

exit $ret
