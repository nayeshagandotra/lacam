import pandas as pd
import matplotlib.pyplot as plt

def plot_soc(csv_file):
    """
    Plots average SOC (Sum of Costs) vs Opti_Deadline for Bool_Opti=True and Bool_Opti=False,
    including scatterplots to show variation.

    Args:
        csv_file (str): Path to the CSV file containing the data.
    """
    # Read the CSV file into a DataFrame
    df = pd.read_csv(csv_file)

    # Separate data for Bool_Opti=True and Bool_Opti=False
    df_true = df[df["Bool_Opti"] == True]
    df_false = df[df["Bool_Opti"] == False]

    # Group by Opti_Deadline and calculate mean and standard deviation for SOC
    avg_true = df_true.groupby("Opti_Deadline")["soc"].mean()
    std_true = df_true.groupby("Opti_Deadline")["soc"].std()
    avg_false = df_false.groupby("Opti_Deadline")["soc"].mean()
    std_false = df_false.groupby("Opti_Deadline")["soc"].std()

    # Plotting
    plt.figure(figsize=(12, 8))

    # Scatterplot for individual points (variation)
    plt.scatter(df_false["Opti_Deadline"], df_false["soc"], label="Bool_Opti=False (Individual)", alpha=0.5, color="blue")
    plt.scatter(df_true["Opti_Deadline"], df_true["soc"], label="Bool_Opti=True (Individual)", alpha=0.5, color="orange")

    # Line plot for averages
    plt.plot(avg_false.index, avg_false.values, label="Bool_Opti=False (Average)", marker="o", color="blue", linestyle="-")
    plt.plot(avg_true.index, avg_true.values, label="Bool_Opti=True (Average)", marker="o", color="orange", linestyle="-")

    # Add error bars to show standard deviation
    plt.errorbar(avg_false.index, avg_false.values, yerr=std_false.values, fmt="o", color="blue", capsize=5, label="Bool_Opti=False (Std Dev)")
    plt.errorbar(avg_true.index, avg_true.values, yerr=std_true.values, fmt="o", color="orange", capsize=5, label="Bool_Opti=True (Std Dev)")

    # Add labels, title, legend, and grid
    plt.xlabel("Opti_Deadline (ms)")
    plt.ylabel("Sum of Costs (SOC)")
    plt.title("SOC vs Opti_Deadline with Variation")
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.xscale('log')

    # Save the plot as an image
    plt.savefig("soc_vs_opti_deadline_with_variation.png", dpi=300, bbox_inches="tight")

def plot_makespan(csv_file, field):
    """
    Plots average SOC (Sum of Costs) vs Opti_Deadline for Bool_Opti=True and Bool_Opti=False,
    including scatterplots to show variation.

    Args:
        csv_file (str): Path to the CSV file containing the data.
    """
    # Read the CSV file into a DataFrame
    df = pd.read_csv(csv_file)
    df = df[df["agents"] == 500]
    columns = df.columns.to_list()
    df[field] = df[field]/df[columns[columns.index(field) + 1]]

    # Separate data for Bool_Opti=True and Bool_Opti=False
    df_true = df[df["Bool_Opti"] == True]
    df_false = df[df["Bool_Opti"] == False]

    # Group by Opti_Deadline and calculate mean and standard deviation for SOC
    avg_true = df_true.groupby("Opti_Deadline")[field].mean()
    std_true = df_true.groupby("Opti_Deadline")[field].std()
    avg_false = df_false.groupby("Opti_Deadline")[field].mean()
    std_false = df_false.groupby("Opti_Deadline")[field].std()

    # Plotting
    plt.figure(figsize=(12, 8))

    # Scatterplot for individual points (variation)
    # plt.scatter(df_false["Opti_Deadline"], df_false[field], label="Bool_Opti=False (Individual)", alpha=0.5, color="blue")
    # plt.scatter(df_true["Opti_Deadline"], df_true[field], label="Bool_Opti=True (Individual)", alpha=0.5, color="orange")

    # Line plot for averages
    plt.plot(avg_false.index, avg_false.values, label="Bool_Opti=False (Average)", marker="o", color="blue", linestyle="-")
    plt.plot(avg_true.index, avg_true.values, label="Bool_Opti=True (Average)", marker="o", color="orange", linestyle="-")

    # Add error bars to show standard deviation
    # plt.errorbar(avg_false.index, avg_false.values, yerr=std_false.values, fmt="o", color="blue", capsize=5, label="Bool_Opti=False (Std Dev)")
    # plt.errorbar(avg_true.index, avg_true.values, yerr=std_true.values, fmt="o", color="orange", capsize=5, label="Bool_Opti=True (Std Dev)")

    # Add labels, title, legend, and grid
    plt.xlabel("Opti_Deadline (ms)")
    plt.ylabel(field)
    plt.title("SOC/SOC_LB vs Opti_Deadline with Variation")
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.xscale('log')

    # Save the plot as an image
    plt.savefig("plot.png", dpi=300, bbox_inches="tight")

if __name__ == "__main__":
    # Path to the CSV file
    csv_file_path = "outputs/lacam_den520d_agentsweep.csv"

    # Plot sum of costs vs opti_deadline with variation
    plot_makespan(csv_file_path, "soc")
