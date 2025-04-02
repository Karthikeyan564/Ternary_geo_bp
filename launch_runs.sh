#!/bin/bash

# Function to limit background processes
tasks=0
max_tasks=4

# Function to run the command with specified flags
run_command() {
    local trace=$1
    local l_flag=$2
    local u_value=$3

    if [[ -z "$l_flag" ]]; then
        # Run without -l or -u flags
        command="./cbp -E 1000000 \"$trace\""
        echo "Launching: $command"
        eval "$command > /dev/null &"
    else
        # Run with -l flag and -u flag
        command="./cbp -l -u \"$u_value\" -E 1000000 \"$trace\""
        echo "Launching: $command"
        eval "$command > /dev/null &"
    fi
}

# Function to wait for background jobs if the task limit is reached
wait_if_needed() {
    if [[ $tasks -ge $max_tasks ]]; then
        wait -n  # Wait for at least one background process to finish
        ((tasks--))
    fi
}

# Run for integer traces
for trace in sample_traces/int/int_*_trace.gz; do
    run_command "$trace" "" ""  # Run without -l or -u
    ((tasks++))
    wait_if_needed

    for u in {1..3}; do
        run_command "$trace" "-l" "$u"  # Run with -l and -u
        ((tasks++))
        wait_if_needed
    done
done

# Run for floating point traces
for trace in sample_traces/fp/fp_*_trace.gz; do
    run_command "$trace" "" ""  # Run without -l or -u
    ((tasks++))
    wait_if_needed

    for u in {1..3}; do
        run_command "$trace" "-l" "$u"  # Run with -l and -u
        ((tasks++))
        wait_if_needed
    done
done

wait  # Ensure all processes complete before script exits
