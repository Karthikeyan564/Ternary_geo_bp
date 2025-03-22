#!/bin/bash

# Generate all trace file paths
trace_files=()
for i in {0..36}; do trace_files+=("int int_${i}_trace.gz"); done
for i in {0..7}; do trace_files+=("compress compress_${i}_trace.gz"); done
for i in {0..9}; do trace_files+=("fp fp_${i}_trace.gz"); done
for i in {0..9}; do trace_files+=("infra infra_${i}_trace.gz"); done
for i in {0..3}; do trace_files+=("media media_${i}_trace.gz"); done
for i in {0..25}; do trace_files+=("web web_${i}_trace.gz"); done

# Create log directory if it doesn't exist
mkdir -p output

# Export the command for xargs
run_cbp() {
    category=$1
    file=$2
    trace_path="../traces/traces/${category}/${file}"
    log_dir="output/${file%.gz}"  # Remove .gz extension
    log_file="${log_dir}/${file%.gz}_bash.log"

    mkdir -p "$log_dir"  # Create log directory if it doesn't exist
    ./cbp -E 1000000 "$trace_path" &> "$log_file"
}

export -f run_cbp

# Run up to 14 processes in parallel
printf "%s\n" "${trace_files[@]}" | xargs -P 14 -n 2 bash -c 'run_cbp "$@"' _
