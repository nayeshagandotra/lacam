import pandas as pd
import matplotlib.pyplot as plt

def find_valid_n(input_csv, required_samples):
    # Load the dataset
    df = pd.read_csv(input_csv)

    # Group by N and count the number of samples for each N
    n_counts = df.groupby('N').size()

    # Filter to find N values with sufficient samples
    valid_n = n_counts[n_counts >= required_samples]

    if valid_n.empty:
        print(f"No value of N has at least {required_samples} samples.")
    else:
        print("The following values of N meet the sample requirement:")
        print(valid_n)

def plot_makespan(csv_file, field, x_axis):
    """
    Plots average SOC (Sum of Costs) vs Opti_Deadline for Bool_Opti=True and Bool_Opti=False,
    including scatterplots to show variation.

    Args:
        csv_file (str): Path to the CSV file containing the data.
    """
    # Read the CSV file into a DataFrame
    df = pd.read_csv(csv_file)
    # df = df[df["Opti_Deadline"] == 1000]
    df = df[df["agents"] <= 260]
    columns = df.columns.to_list()
    df[field] = df[field]/df[columns[columns.index(field) + 1]]

    # Separate data for Bool_Opti=True and Bool_Opti=False
    df_true = df[df["Bool_Opti"] == True]
    df_false = df[df["Bool_Opti"] == False]

    avg_true = df_true.groupby(f"{x_axis}")[field].mean()
    std_true = df_true.groupby(f"{x_axis}")[field].std()
    avg_false = df_false.groupby(f"{x_axis}")[field].mean()
    std_false = df_false.groupby(f"{x_axis}")[field].std()

    # Plotting
    plt.figure(figsize=(12, 8))

    # Scatterplot for individual points (variation)
    # plt.scatter(df_false[f"{x_axis}"], df_false[field], label="Bool_Opti=False (Individual)", alpha=0.5, color="blue")
    # plt.scatter(df_true[f"{x_axis}"], df_true[field], label="Bool_Opti=True (Individual)", alpha=0.5, color="orange")

    # Line plot for averages
    plt.plot(avg_false.index, avg_false.values, label="Bool_Opti=False (Average)", marker="o", color="blue", linestyle="-")
    plt.plot(avg_true.index, avg_true.values, label="Bool_Opti=True (Average)", marker="o", color="orange", linestyle="-")

    # Add error bars to show standard deviation
    # plt.errorbar(avg_false.index, avg_false.values, yerr=std_false.values, fmt="o", color="blue", capsize=5, label="Bool_Opti=False (Std Dev)")
    # plt.errorbar(avg_true.index, avg_true.values, yerr=std_true.values, fmt="o", color="orange", capsize=5, label="Bool_Opti=True (Std Dev)")

    # Add labels, title, legend, and grid
    plt.xlabel(f"{x_axis}")
    plt.ylabel(field)
    plt.title(f"{field} vs Opti_Deadline with Variation")
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.7)
    # plt.xscale('log')

    # Save the plot as an image
    plt.savefig("plot.png", dpi=300, bbox_inches="tight")

if __name__ == "__main__":
    # Path to the CSV file
    csv_file_path = "outputs/lacam_323220_agentsweep.csv"

    # Plot sum of costs vs opti_deadline with variation
    plot_makespan(csv_file_path, "soc", "Opti_Deadline")
    find_valid_n(csv_file_path, 25*2*5)
