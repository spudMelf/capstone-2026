#!/usr/bin/env python3
"""Create received-power plots from the capstone simulation CSV files."""

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


CAPSTONE_DIR = Path(__file__).resolve().parent
RESULTS_DIR = CAPSTONE_DIR / "results"
PLOTS_DIR = RESULTS_DIR / "plots"
FRIIS_CSV = RESULTS_DIR / "friis-results.csv"
LOG_DISTANCE_CSV = RESULTS_DIR / "log-distance-results.csv"
FRIIS_TX_POWER_CSV = RESULTS_DIR / "friis-tx-power-results.csv"
LOG_DISTANCE_TX_POWER_CSV = RESULTS_DIR / "log-distance-n4-tx-power-results.csv"
ANTENNA_MODEL_CSV = RESULTS_DIR / "antenna-model-comparison-results.csv"
ANTENNA_TX_POWER_CSV = RESULTS_DIR / "antenna-model-tx-power-results.csv"
MIXED_ANTENNA_TX_POWER_CSV = RESULTS_DIR / "parabolic-vs-isotropic-tx-power-results.csv"
LOG_DISTANCE_EXPONENT = 4.0


def configure_plot_style() -> None:
    plt.rcParams.update(
        {
            "figure.dpi": 120,
            "savefig.dpi": 200,
            "font.size": 11,
            "axes.titlesize": 15,
            "axes.labelsize": 12,
            "axes.grid": True,
            "grid.alpha": 0.25,
            "legend.frameon": False,
        }
    )


def save_distance_plot(
    data: pd.DataFrame,
    title: str,
    output_name: str,
    color: str,
) -> None:
    ordered = data.sort_values("distance_m")
    figure, axis = plt.subplots(figsize=(8.5, 5.5))
    axis.plot(
        ordered["distance_m"],
        ordered["predicted_rx_power_dbm"],
        marker="o",
        linewidth=2.2,
        markersize=6,
        color=color,
    )
    axis.set_title(title)
    axis.set_xlabel("Transmitter–receiver distance (m)")
    axis.set_ylabel("Predicted received power (dBm)")
    axis.set_xticks(ordered["distance_m"])
    figure.tight_layout()
    figure.savefig(PLOTS_DIR / output_name, bbox_inches="tight")
    plt.close(figure)


def save_tx_power_plot(
    data: pd.DataFrame,
    title: str,
    output_name: str,
    color: str,
) -> None:
    ordered = data.sort_values("tx_power_dbm")
    figure, axis = plt.subplots(figsize=(8.5, 5.5))
    axis.plot(
        ordered["tx_power_dbm"],
        ordered["predicted_rx_power_dbm"],
        marker="o",
        linewidth=2.2,
        markersize=6,
        color=color,
    )
    axis.set_title(title)
    axis.set_xlabel("Transmit power (dBm)")
    axis.set_ylabel("Predicted received power (dBm)")
    axis.set_xticks(ordered["tx_power_dbm"])
    axis.set_xlim(-21, 11)
    axis.set_ylim(-100, -50)
    figure.tight_layout()
    figure.savefig(PLOTS_DIR / output_name, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    friis = pd.read_csv(FRIIS_CSV)
    log_distance = pd.read_csv(LOG_DISTANCE_CSV)
    friis_tx_power = pd.read_csv(FRIIS_TX_POWER_CSV)
    log_distance_tx_power = pd.read_csv(LOG_DISTANCE_TX_POWER_CSV)
    antenna_models = pd.read_csv(ANTENNA_MODEL_CSV)
    antenna_tx_power = pd.read_csv(ANTENNA_TX_POWER_CSV)
    mixed_antenna_tx_power = pd.read_csv(MIXED_ANTENNA_TX_POWER_CSV)

    required_columns = {"distance_m", "path_loss_exponent", "predicted_rx_power_dbm"}
    missing_log_columns = required_columns.difference(log_distance.columns)
    if missing_log_columns:
        raise ValueError(f"Log-distance results are missing: {sorted(missing_log_columns)}")
    if not {"distance_m", "predicted_rx_power_dbm"}.issubset(friis.columns):
        raise ValueError("Friis results do not contain the required plotting columns")

    configure_plot_style()

    save_distance_plot(
        friis,
        "Friis model: received power vs. distance",
        "friis-received-power-vs-distance.png",
        "#2563A6",
    )

    selected_exponent = log_distance[
        log_distance["path_loss_exponent"].eq(LOG_DISTANCE_EXPONENT)
    ]
    if selected_exponent.empty:
        raise ValueError(f"No log-distance rows found for n={LOG_DISTANCE_EXPONENT:g}")
    save_distance_plot(
        selected_exponent,
        f"Log-distance model (n={LOG_DISTANCE_EXPONENT:g}): received power vs. distance",
        "log-distance-n4-received-power-vs-distance.png",
        "#D97706",
    )

    figure, axis = plt.subplots(figsize=(8.5, 5.8))
    for distance_m, distance_rows in log_distance.groupby("distance_m", sort=True):
        ordered = distance_rows.sort_values("path_loss_exponent")
        axis.plot(
            ordered["path_loss_exponent"],
            ordered["predicted_rx_power_dbm"],
            marker="o",
            linewidth=2,
            markersize=5,
            label=f"{distance_m:g} m",
        )

    axis.set_title("Log-distance model: received power vs. path-loss exponent")
    axis.set_xlabel("Path-loss exponent, n")
    axis.set_ylabel("Predicted received power (dBm)")
    axis.set_xticks(sorted(log_distance["path_loss_exponent"].unique()))
    axis.legend(title="Distance", ncol=2)
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "log-distance-received-power-vs-exponent.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    tx_power_columns = {"tx_power_dbm", "predicted_rx_power_dbm", "distance_m"}
    for name, data in (
        ("Friis", friis_tx_power),
        ("Log-distance", log_distance_tx_power),
    ):
        missing_columns = tx_power_columns.difference(data.columns)
        if missing_columns:
            raise ValueError(f"{name} transmit-power results are missing: {sorted(missing_columns)}")

    save_tx_power_plot(
        friis_tx_power,
        "Friis model at 5 m: received power vs. transmit power",
        "friis-received-power-vs-tx-power.png",
        "#2563A6",
    )
    save_tx_power_plot(
        log_distance_tx_power,
        "Log-distance model (n=4) at 5 m: received power vs. transmit power",
        "log-distance-n4-received-power-vs-tx-power.png",
        "#D97706",
    )

    figure, axis = plt.subplots(figsize=(8.5, 5.8))
    for data, label, color in (
        (friis_tx_power, "Friis", "#2563A6"),
        (log_distance_tx_power, "Log-distance, n=4", "#D97706"),
    ):
        ordered = data.sort_values("tx_power_dbm")
        axis.plot(
            ordered["tx_power_dbm"],
            ordered["predicted_rx_power_dbm"],
            marker="o",
            linewidth=2.2,
            markersize=6,
            label=label,
            color=color,
        )
    axis.set_title("Received power vs. transmit power at 5 m")
    axis.set_xlabel("Transmit power (dBm)")
    axis.set_ylabel("Predicted received power (dBm)")
    axis.set_xticks(sorted(friis_tx_power["tx_power_dbm"].unique()))
    axis.set_xlim(-21, 11)
    axis.set_ylim(-100, -50)
    axis.legend()
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "friis-vs-log-distance-n4-tx-power.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    antenna_columns = {
        "propagation_case",
        "antenna_model",
        "pointing_error_deg",
        "tx_pattern_gain_dbi",
        "predicted_rx_power_dbm",
        "pdr_percent",
    }
    missing_antenna_columns = antenna_columns.difference(antenna_models.columns)
    if missing_antenna_columns:
        raise ValueError(
            f"Antenna-model results are missing: {sorted(missing_antenna_columns)}"
        )

    direct_los = antenna_models[
        antenna_models["propagation_case"].eq("FriisDirectLos")
    ]
    model_colors = {
        "Isotropic": "#64748B",
        "Cosine": "#2563A6",
        "Parabolic": "#D97706",
        "ThreeGppIndoor": "#059669",
    }
    model_styles = {
        "Isotropic": ("o", "-"),
        "Cosine": ("o", "-"),
        "Parabolic": ("s", "--"),
        "ThreeGppIndoor": ("x", ":"),
    }

    figure, axis = plt.subplots(figsize=(8.8, 5.8))
    for antenna_model, rows in direct_los.groupby("antenna_model", sort=False):
        ordered = rows.sort_values("pointing_error_deg")
        relative_gain = ordered["tx_pattern_gain_dbi"] - ordered["tx_pattern_gain_dbi"].iloc[0]
        marker, line_style = model_styles[antenna_model]
        axis.plot(
            ordered["pointing_error_deg"],
            relative_gain,
            marker=marker,
            linestyle=line_style,
            linewidth=2.1,
            markersize=7,
            label=antenna_model,
            color=model_colors[antenna_model],
        )
    axis.set_title("Native ns-3 antenna patterns: gain vs. pointing error")
    axis.set_xlabel("Pointing error at each antenna (degrees)")
    axis.set_ylabel("Gain relative to boresight (dB per antenna)")
    axis.set_xticks(sorted(direct_los["pointing_error_deg"].unique()))
    axis.legend()
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "antenna-pattern-gain-vs-pointing-error.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    figure, axis = plt.subplots(figsize=(8.8, 5.8))
    for antenna_model, rows in direct_los.groupby("antenna_model", sort=False):
        ordered = rows.sort_values("pointing_error_deg")
        marker, line_style = model_styles[antenna_model]
        axis.plot(
            ordered["pointing_error_deg"],
            ordered["predicted_rx_power_dbm"],
            marker=marker,
            linestyle=line_style,
            linewidth=2.1,
            markersize=7,
            label=antenna_model,
            color=model_colors[antenna_model],
        )
    axis.axhline(-101.0, color="#991B1B", linestyle="--", linewidth=1.5, label="Rx sensitivity")
    axis.set_title("One-metre cryostat-window link: received power vs. pointing error")
    axis.set_xlabel("Pointing error at each antenna (degrees)")
    axis.set_ylabel("Predicted received power (dBm)")
    axis.set_xticks(sorted(direct_los["pointing_error_deg"].unique()))
    axis.legend(ncol=2)
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "antenna-model-received-power-vs-pointing-error.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    boresight = antenna_models[antenna_models["pointing_error_deg"].eq(0.0)]
    reflection_pivot = boresight.pivot(
        index="antenna_model",
        columns="propagation_case",
        values="predicted_rx_power_dbm",
    ).reindex(model_colors.keys())
    reflection_delta = (
        reflection_pivot["TwoRayGroundExploratory"]
        - reflection_pivot["FriisDirectLos"]
    )
    figure, axis = plt.subplots(figsize=(8.8, 5.6))
    x_positions = range(len(reflection_delta))
    axis.axhline(0.0, color="#64748B", linewidth=1.4)
    axis.scatter(x_positions, reflection_delta, color="#2563A6", s=70, zorder=3)
    for x_position, difference_db in zip(x_positions, reflection_delta):
        axis.annotate(
            f"{difference_db:.3f} dB",
            (x_position, difference_db),
            xytext=(0, 10),
            textcoords="offset points",
            ha="center",
        )
    axis.set_title("Two-ray-ground minus Friis received power at 1 m")
    axis.set_xlabel("Antenna model")
    axis.set_ylabel("Received-power difference (dB)")
    axis.set_xticks(list(x_positions), reflection_delta.index)
    axis.set_ylim(-0.25, 0.25)
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "antenna-model-reflection-sensitivity.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    tx_power_antenna_columns = {
        "antenna_model",
        "pointing_error_deg",
        "tx_power_dbm",
        "predicted_rx_power_dbm",
    }
    missing_tx_power_antenna_columns = tx_power_antenna_columns.difference(
        antenna_tx_power.columns
    )
    if missing_tx_power_antenna_columns:
        raise ValueError(
            "Antenna transmit-power results are missing: "
            f"{sorted(missing_tx_power_antenna_columns)}"
        )

    figure, axes = plt.subplots(1, 2, figsize=(12.5, 5.8), sharey=True)
    for axis, pointing_error_deg in zip(axes, (0.0, 60.0)):
        selected = antenna_tx_power[
            antenna_tx_power["pointing_error_deg"].eq(pointing_error_deg)
        ]
        for antenna_model, rows in selected.groupby("antenna_model", sort=False):
            ordered = rows.sort_values("tx_power_dbm")
            marker, line_style = model_styles[antenna_model]
            axis.plot(
                ordered["tx_power_dbm"],
                ordered["predicted_rx_power_dbm"],
                marker=marker,
                linestyle=line_style,
                linewidth=2.1,
                markersize=6,
                label=antenna_model,
                color=model_colors[antenna_model],
            )
        axis.axhline(
            -101.0,
            color="#991B1B",
            linestyle="--",
            linewidth=1.4,
            label="Rx sensitivity",
        )
        axis.set_title(
            "Boresight aligned"
            if pointing_error_deg == 0.0
            else "60° error at each antenna"
        )
        axis.set_xlabel("Transmit power (dBm)")
        axis.set_xticks(sorted(selected["tx_power_dbm"].unique()))
        axis.set_ylim(-105, -25)
    axes[0].set_ylabel("Predicted received power (dBm)")
    handles, labels = axes[1].get_legend_handles_labels()
    figure.legend(handles, labels, loc="upper center", ncol=5, bbox_to_anchor=(0.5, 0.98))
    figure.suptitle("Antenna models: received power vs. transmit power", y=1.03)
    figure.tight_layout(rect=(0, 0, 1, 0.91))
    figure.savefig(
        PLOTS_DIR / "antenna-model-received-power-vs-tx-power.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    mixed_tx_power_columns = {
        "tx_antenna_model",
        "distance_m",
        "tx_power_dbm",
        "predicted_rx_power_dbm",
        "pdr_percent",
    }
    missing_mixed_columns = mixed_tx_power_columns.difference(
        mixed_antenna_tx_power.columns
    )
    if missing_mixed_columns:
        raise ValueError(
            "Mixed-antenna transmit-power results are missing: "
            f"{sorted(missing_mixed_columns)}"
        )
    if not mixed_antenna_tx_power["distance_m"].eq(1.0).all():
        raise ValueError("Mixed-antenna transmit-power sweep must remain at 1 m")

    comparison_colors = {"Parabolic": "#D97706", "Isotropic": "#2563A6"}
    comparison_styles = {"Parabolic": ("o", "-"), "Isotropic": ("s", "--")}

    figure, axis = plt.subplots(figsize=(8.8, 5.8))
    for antenna_model in ("Parabolic", "Isotropic"):
        ordered = mixed_antenna_tx_power[
            mixed_antenna_tx_power["tx_antenna_model"].eq(antenna_model)
        ].sort_values("tx_power_dbm")
        marker, line_style = comparison_styles[antenna_model]
        label = f"{antenna_model} Tx"
        axis.plot(
            ordered["tx_power_dbm"],
            ordered["predicted_rx_power_dbm"],
            marker=marker,
            linestyle=line_style,
            linewidth=2.1,
            markersize=5,
            label=label,
            color=comparison_colors[antenna_model],
        )
    axis.axhline(
        -82.0,
        color="#64748B",
        linestyle=":",
        linewidth=1.5,
        label="Preamble minimum RSSI",
    )
    axis.set_title("Received power vs. transmit power at 1 m")
    axis.set_xlabel("Transmit power (dBm)")
    axis.set_ylabel("Predicted received power (dBm)")
    axis.legend()
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "parabolic-vs-isotropic-tx-power-received-power.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    figure, axis = plt.subplots(figsize=(8.8, 5.8))
    for antenna_model in ("Parabolic", "Isotropic"):
        ordered = mixed_antenna_tx_power[
            mixed_antenna_tx_power["tx_antenna_model"].eq(antenna_model)
        ].sort_values("tx_power_dbm")
        marker, line_style = comparison_styles[antenna_model]
        axis.plot(
            ordered["tx_power_dbm"],
            ordered["pdr_percent"],
            marker=marker,
            linestyle=line_style,
            linewidth=2.1,
            markersize=5,
            label=f"{antenna_model} Tx",
            color=comparison_colors[antenna_model],
        )
        reliable = ordered[ordered["pdr_percent"].ge(99.0)]
        if not reliable.empty:
            first_reliable_power = reliable.iloc[0]["tx_power_dbm"]
            failed_below = ordered[
                ordered["tx_power_dbm"].lt(first_reliable_power)
                & ordered["pdr_percent"].lt(99.0)
            ]
            if not failed_below.empty:
                last_failed_power = failed_below.iloc[-1]["tx_power_dbm"]
                axis.annotate(
                    f"{antenna_model} cliff\n{last_failed_power:g} to "
                    f"{first_reliable_power:g} dBm",
                    (first_reliable_power, 100.0),
                    xytext=(first_reliable_power + 5,
                            72 if antenna_model == "Parabolic" else 42),
                    textcoords="data",
                    arrowprops={
                        "arrowstyle": "->",
                        "color": comparison_colors[antenna_model],
                    },
                    color=comparison_colors[antenna_model],
                )
    axis.set_title("Packet delivery vs. transmit power at 1 m")
    axis.set_xlabel("Transmit power (dBm)")
    axis.set_ylabel("Packet delivery ratio (%)")
    axis.set_ylim(-5, 105)
    axis.legend()
    figure.tight_layout()
    figure.savefig(
        PLOTS_DIR / "parabolic-vs-isotropic-tx-power-pdr.png",
        bbox_inches="tight",
    )
    plt.close(figure)

    print(f"Plots saved to {PLOTS_DIR}")


if __name__ == "__main__":
    main()
