import subprocess
import pandas as pd
import os
import re
import argparse
import time

def run_lacam(args, opti_deadline, bool_opti):
    """
    Runs the lacam executable with the given arguments, opti_deadline, and bool_opti flag.
    """
    # Add the bool_opti argument
    opti_flag = "--bool_opti=true" if bool_opti else ""
    opti_deadline_arg = f"--opti_deadline={opti_deadline}"

    # Construct the command
    command = [args["program"]] + args["args"] + [opti_flag, opti_deadline_arg]

    # print(f"Running command: {' '.join(command)}")
    
    try:
        # Run the subprocess and capture output
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        return result.stdout.strip(), result.stderr.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error while running lacam: {e}")
        return e.stdout.strip(), e.stderr.strip()

def parse_result_txt(result_file):
    """
    Parses the result.txt file and extracts relevant fields.
    """
    patterns = {
        "agents": r"agents=(\d+)",
        "map_file": r"map_file=([\w\-.]+)",
        "solver": r"solver=([\w\-.]+)",
        "solved": r"solved=(\d+)",
        "soc": r"soc=(\d+)",
        "soc_lb": r"soc_lb=(\d+)",
        "makespan": r"makespan=(\d+)",
        "makespan_lb": r"makespan_lb=(\d+)",
        "sum_of_loss": r"sum_of_loss=(\d+)",
        "sum_of_loss_lb": r"sum_of_loss_lb=(\d+)",
        "comp_time": r"comp_time=(\d+)",
        "seed": r"seed=(\d+)"
    }

    extracted_data = {}

    try:
        with open(result_file, 'r') as file:
            content = file.read()

            for field, pattern in patterns.items():
                match = re.search(pattern, content)
                if match:
                    extracted_data[field] = int(match.group(1)) if match.group(1).isdigit() else match.group(1)

    except FileNotFoundError:
        print(f"Error: {result_file} not found.")

    return extracted_data

def cost_processor(cost_file, n):
    """
    Reads costs from costs.txt, processes them, and deletes the costs.txt file.
    """
    if not os.path.exists(cost_file):
        print(f"Warning: {cost_file} not found. Skipping cost processing.")
        return None  # Return None if no costs found
    
    # Read costs from the file
    costs = []
    with open(cost_file, 'r') as file:
        for line in file:
            if line.strip().isdigit():
                costs.append(int(line.strip()))

    if not costs:
        print(f"Warning: No valid costs found in {cost_file}.")
        return None

    # Add costs to the results (assuming costs alternate between cost1 and cost2)
    results = []
    cost = 0
    for i in range(0, len(costs)):
        cost = costs[i]
        results.append({f"Cost_{n}": cost})

    # Clean up: delete the costs file after processing
    # os.remove(cost_file)
    # print(f"Costs processed, {cost_file} deleted.")

    return results  # Return the processed cost data

def lacam_batch_runner(output_csv, map_folder, result_file, max_time_threshold=50):
    """
    Runs lacam with varying opti_deadline values and logs results in a CSV file.
    Loops through different number of agents (N), checks if execution time exceeds threshold, and stops early if needed.
    """
    # Arguments for lacam from launch.json
    args = {
        "program": "build/main",  # Path to your compiled binary
        "args": [
            "-v", "1"
        ]
    }

    # Get all .map files in the map folder
    map_files = [f for f in os.listdir(map_folder) if f.endswith('.map')]
    if not map_files:
        print(f"No .map files found in {map_folder}. Please check the folder path.")
        return
    
    # Use the first map file (you can modify this part if you need to choose specific map files)
    map_file_path = os.path.join(map_folder, map_files[0])
    args["args"].append(f"-m {map_file_path}")   # Add the map file

    results = []

    # Get all .scen files in the map folder
    scen_files = [f for f in os.listdir(map_folder) if f.endswith('.scen')]
    if not scen_files:
        print(f"No .scen files found in {map_folder}. Please check the folder path.")
        return
    
    max_n_limit = 200  # Initially, no limit

    # Check the maximum processed agents for the current scenario
    existing_data = pd.read_csv(output_csv) if os.path.exists(output_csv) else pd.DataFrame()

    # Loop through all .scen files in the map folder
    for scen_file in scen_files:
        scen_file_path = os.path.join(map_folder, scen_file)
        
        max_processed_n = 20
        skip_loop = False

        if not existing_data.empty:
            scenario_data = existing_data[existing_data["Scenario_File"] == scen_file]
            if not scenario_data.empty:
                max_processed_n = scenario_data["N"].max()
                if max_processed_n >= max_n_limit:
                    continue
        
        # Loop through different numbers of agents (N) using range(20, 600, 40)
        for N in range(max_processed_n, max_n_limit + 1, 40):  # Adjust these numbers as needed
            for bool_opti in [False, True]:  # Run for both False and True
                opti_deadline = 0
                if bool_opti:
                    for opti_deadline in [0,1,4,16,64,256]:  # From 0ms to 1s inclusive
                        # print(f"Running lacam with N={N}, bool_opti={bool_opti}, opti_deadline={opti_deadline}ms, scenario={scen_file_path})...")
                        # Reset args for each new scenario file
                        args["args"] = [
                            "-v", "1",
                            "-m", f"{map_file_path}",  # Add the map file
                            "-i",  f"{scen_file_path}",  # Add the current scenario file
                            "-o", f"{result_file}"
                        ]
                        # Update the number of agents (N)
                        args["args"] = ["-N", f"{N}"] + args["args"][1:]

                        start_time = time.time()  # Start timer

                        # Run lacam executable
                        stdout, stderr = run_lacam(args, opti_deadline, bool_opti)

                        # Calculate the elapsed time
                        elapsed_time = time.time() - start_time

                        if elapsed_time > max_time_threshold:
                            print(f"Experiment with {N} agents took {elapsed_time:.2f} seconds, which exceeds the threshold of {max_time_threshold} seconds.")
                            # Update the max_n_limit to stop further larger agent numbers for this and subsequent scenarios
                            max_n_limit = N  # Set the new limit to the last successful N value
                            print(f"scen_file: {scen_file}, max_n_limit: {max_n_limit}")
                            skip_loop = True
                            break  # Skip the remaining N values for this scenario

                        # Parse the result file for output data
                        parsed_data = parse_result_txt(result_file)
                        
                        if not parsed_data:
                            print(f"Skipping experiment due to missing or invalid {result_file} for {scen_file}.")
                            continue
                        
                        # Add additional fields to parsed data
                        parsed_data["Opti_Deadline"] = opti_deadline
                        parsed_data["Bool_Opti"] = bool_opti
                        parsed_data["Scenario_File"] = scen_file
                        parsed_data["N"] = N

                        df = pd.DataFrame([parsed_data])
                        df.to_csv(output_csv, mode='a', header=not os.path.exists(output_csv), index=False)
                        # print(f"Results for {scen_file} with N={N} saved to {output_csv}")
                else: 
                    # print(f"Running lacam with N={N}, bool_opti={bool_opti}, opti_deadline={opti_deadline}ms, scenario={scen_file_path})...")
                    # Reset args for each new scenario file
                    args["args"] = [
                        "-v", "1",
                        "-m", f"{map_file_path}",  # Add the map file
                        "-i",  f"{scen_file_path}",  # Add the current scenario file
                        "-o", f"{result_file}"
                    ]
                    # Update the number of agents (N)
                    args["args"] = ["-N", f"{N}"] + args["args"][1:]

                    start_time = time.time()  # Start timer

                    # Run lacam executable
                    stdout, stderr = run_lacam(args, opti_deadline, bool_opti)

                    # Calculate the elapsed time
                    elapsed_time = time.time() - start_time

                    if elapsed_time > max_time_threshold:
                        print(f"Experiment with {N} agents took {elapsed_time:.2f} seconds, which exceeds the threshold of {max_time_threshold} seconds.")
                        # Update the max_n_limit to stop further larger agent numbers for this and subsequent scenarios
                        max_n_limit = N  # Set the new limit to the last successful N value
                        print(f"scen_file: {scen_file}, max_n_limit: {max_n_limit}")
                        skip_loop = True
                        break  # Skip the remaining N values for this scenario

                    # Parse the result file for output data
                    parsed_data = parse_result_txt(result_file)
                    
                    if not parsed_data:
                        print(f"Skipping experiment due to missing or invalid {result_file} for {scen_file}.")
                        continue
                    
                    # Add additional fields to parsed data
                    parsed_data["Opti_Deadline"] = opti_deadline
                    parsed_data["Bool_Opti"] = bool_opti
                    parsed_data["Scenario_File"] = scen_file
                    parsed_data["N"] = N

                    df = pd.DataFrame([parsed_data])
                    df.to_csv(output_csv, mode='a', header=not os.path.exists(output_csv), index=False)
                    # print(f"Results for {scen_file} with N={N} saved to {output_csv}")
                if skip_loop:
                    break
            if skip_loop:
                break

    print("Batch processing complete.")

def lacam_single_runner(output_csv, map_folder, result_file, max_time_threshold, N, bool_opti=True):
    """
    Runs lacam with varying opti_deadline values and logs results in a CSV file.
    Loops through different number of agents (N), checks if execution time exceeds threshold, and stops early if needed.
    """
    # Arguments for lacam from launch.json
    args = {
        "program": "build/main",  # Path to your compiled binary
        "args": [
            "-v", "1"
        ]
    }

    ogo = output_csv

    # Get all .map files in the map folder
    map_files = [f for f in os.listdir(map_folder) if f.endswith('.map')]
    if not map_files:
        print(f"No .map files found in {map_folder}. Please check the folder path.")
        return
    
    # Use the first map file (you can modify this part if you need to choose specific map files)
    map_file_path = os.path.join(map_folder, map_files[0])
    args["args"].append(f"-m {map_file_path}")   # Add the map file

    # Get all .scen files in the map folder
    scen_files = [f for f in os.listdir(map_folder) if f.endswith('.scen')]
    if not scen_files:
        print(f"No .scen files found in {map_folder}. Please check the folder path.")
        return
    
    max_n_limit = 1000  # Initially, no limit

    # Check the maximum processed agents for the current scenario
    existing_data = pd.read_csv(output_csv) if os.path.exists(output_csv) else pd.DataFrame()

    # Loop through all .scen files in the map folder
    for scen_file in scen_files:
        scen_file_path = os.path.join(map_folder, scen_file)
        output_csv = ogo + f"{scen_file}"
        results_df = pd.DataFrame()
        
        max_processed_n = 20
        skip_loop = False

        if not existing_data.empty:
            scenario_data = existing_data[existing_data["Scenario_File"] == scen_file]
            if not scenario_data.empty:
                max_processed_n = scenario_data["N"].max()
                if max_processed_n >= max_n_limit:
                    continue
        
        # Loop through different numbers of agents (N) using range(20, 600, 40)
        for opti_deadline in [0, 1, 10, 100, 1000]:  # From 0ms to 1s inclusive
            # print(f"Running lacam with N={N}, bool_opti={bool_opti}, opti_deadline={opti_deadline}ms, scenario={scen_file_path})...")
            # Reset args for each new scenario file
            args["args"] = [
                "-v", "1",
                "-m", f"{map_file_path}",  # Add the map file
                "-i",  f"{scen_file_path}",  # Add the current scenario file
                "-o", f"{result_file}"
            ]
            # Update the number of agents (N)
            args["args"] = ["-N", f"{N}"] + args["args"][1:]

            start_time = time.time()  # Start timer

            # Run lacam executable
            stdout, stderr = run_lacam(args, opti_deadline, bool_opti)

            # Calculate the elapsed time
            elapsed_time = time.time() - start_time

            if elapsed_time > max_time_threshold:
                print(f"Experiment with {N} agents took {elapsed_time:.2f} seconds, which exceeds the threshold of {max_time_threshold} seconds.")
                # Update the max_n_limit to stop further larger agent numbers for this and subsequent scenarios
                max_n_limit = N  # Set the new limit to the last successful N value
                print(f"scen_file: {scen_file}, max_n_limit: {max_n_limit}")
                skip_loop = True
                break  # Skip the remaining N values for this scenario

            # Parse the result file for output data
            # parsed_data = parse_result_txt(result_file)
            # Parse the result file for output data
            cost_data = cost_processor("costs1.txt", opti_deadline)
            cost_data2 = cost_processor("costs2.txt", opti_deadline)

            if cost_data and cost_data2:
                # Ensure both cost_data lists have the same length
                if len(cost_data) != len(cost_data2):
                    print(f"Warning: Mismatch in lengths of cost_data ({len(cost_data)}) and cost_data2 ({len(cost_data2)}).")
                    # Truncate to the shorter length
                    min_length = min(len(cost_data), len(cost_data2))
                    cost_data = cost_data[:min_length]
                    cost_data2 = cost_data2[:min_length]

                # Combine costs into the DataFrame with distinct columns for each opti_deadline
                combined_data = [
                    {
                        **{"Scenario_File": scen_file, "Opti_Deadline": opti_deadline},
                        f"Cost_1_{opti_deadline}": cost1[f"Cost_{opti_deadline}"],
                        f"Cost_2_{opti_deadline}": cost2[f"Cost_{opti_deadline}"],
                    }
                    for cost1, cost2 in zip(cost_data, cost_data2)
                ]

                # Append the combined data to the results DataFrame
                results_df = pd.concat(
                    [results_df, pd.DataFrame(combined_data)],
                    ignore_index=True
                )

        # Save the results for the scenario to CSV
        results_df.to_csv(output_csv, index=False)
        print(f"Results for {scen_file} saved to {output_csv}.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Batch runner for lacam experiments.")
    parser.add_argument(
        "--output_csv",
        type=str,
        required=False,
        help="Path to the output CSV file where results will be saved."
    )
    parser.add_argument(
        "--map_folder",
        type=str,
        required=False,
        help="Path to the folder containing map and scenario files."
    )
    parser.add_argument(
        "--result_file",
        type=str,
        required=False,
        help="Path to the result.txt file generated by lacam."
    )
    parser.add_argument(
        "--max_time_threshold",
        type=int,
        default=60,
        help="Maximum time (in seconds) allowed for an experiment. Default is 50 seconds."
    )

    args = parser.parse_args()

    # Run the batch runner
    lacam_batch_runner(
        output_csv=args.output_csv,
        map_folder=args.map_folder,
        result_file=args.result_file,
        max_time_threshold=args.max_time_threshold
    )
    # lacam_single_runner(
    #     output_csv=args.output_csv,
    #     map_folder=args.map_folder,
    #     result_file=args.result_file,
    #     max_time_threshold=args.max_time_threshold,
    #     N=500,
    #     bool_opti=True
    # )


