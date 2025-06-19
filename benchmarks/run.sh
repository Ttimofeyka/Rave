#!/bin/bash

# Check if benchmark name is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <benchmark_name>"
    exit 1
fi

benchmark_name=$1
benchmark_path="./benchmarks/$benchmark_name"

# Check if the benchmark file exists and is executable
if [ ! -x "$benchmark_path" ]; then
    echo "Error: Benchmark file '$benchmark_path' does not exist or is not executable."
    exit 1
fi

# Array to store the real times
real_times=()

# Run the command 10 times and capture the real time
for ((i=1; i<=10; i++))
do
    # Run the command and capture the output
    time_output=$({ time "$benchmark_path"; } 2>&1)

    # Extract the real time using awk
    real_time=$(echo "$time_output" | awk '/real/ {print $2}')

    # Store the real time in the array
    real_times+=("$real_time")
done

# Function to convert time in mm:ss format to seconds
convert_to_seconds() {
    local time=$1
    local mm=$(echo "$time" | cut -d'm' -f1)
    local ss=$(echo "$time" | cut -d'm' -f2 | cut -d's' -f1)
    echo "$(bc <<< "$mm * 60 + $ss")"
}

# Convert all times to seconds
real_times_seconds=()
for time in "${real_times[@]}"
do
    real_times_seconds+=($(convert_to_seconds "$time"))
done

# Find the best and worst times
best_time=$(printf "%s\n" "${real_times_seconds[@]}" | sort -n | head -1)
worst_time=$(printf "%s\n" "${real_times_seconds[@]}" | sort -n | tail -1)

# Output the results
echo "Best time: $best_time seconds"
echo "Worst time: $worst_time seconds"
