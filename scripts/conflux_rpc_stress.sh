#!/bin/bash

# Find all conflux.conf files matching the pattern
find /tmp/conflux_test_* -name "conflux.conf" | while read -r conf_file; do
    # Extract the port number
    port=$(grep "jsonrpc_local_http_port=" "$conf_file" | cut -d'=' -f2)

    # Get the directory containing the conf file
    conf_dir=$(dirname "$conf_file")

    # Create a log file name
    log_file="$conf_dir/rpc_stress.log"

    # Check if port was found
    if [[ -n "$port" ]]; then
        echo "Running stress test for config: $conf_file with port: $port"
        ./conflux-rpc-stress --url http://localhost:$port -m 10 > "$log_file"
    else
        echo "No port found in $conf_file, skipping..."
    fi
done