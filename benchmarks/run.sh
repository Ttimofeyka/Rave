#!/bin/bash

# Check if input file is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 INPUT_FILE"
    exit 1
fi

input_file="$1"
times=()

# Check if file exists
if [ ! -f "./benchmarks/$input_file" ]; then
    echo "Error: ./benchmarks/$input_file does not exist"
    exit 1
fi

echo "Running benchmark 10 times for ./benchmarks/$input_file..."

for i in {1..10}; do
    echo -n "Run $i: "
    # Use 'time' in a subshell and capture its output
    # We use the 'real' time, which is the wall clock time
    time_output=$( { time -p ./benchmarks/"$input_file"; } 2>&1 )
    # Extract the real time value (will be something like "real 1.23")
    real_time=$(echo "$time_output" | grep -E '^real' | awk '{print $2}')
    echo "$real_time seconds"
    times+=("$real_time")
done

# Initialize best and worst with first time
best=${times[0]}
worst=${times[0]}

# Find best and worst times
for time in "${times[@]}"; do
    # Use bc for floating point comparison
    if (( $(echo "$time < $best" | bc -l) )); then
        best=$time
    fi
    if (( $(echo "$time > $worst" | bc -l) )); then
        worst=$time
    fi
done

average=$(echo "scale=3; ($best + $worst) / 2" | bc -l)

echo "-----------------------------"
printf "Best Time:  %.3f seconds\n" "$best"
printf "Worst Time: %.3f seconds\n" "$worst"
printf "Average: %.3f seconds\n" "$average"
