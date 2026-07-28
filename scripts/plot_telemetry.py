#!/usr/bin/env python3
"""Create a portable SVG chart from MotionBridge CSV telemetry.

Uses only Python's standard library so the project has no plotting dependency.
"""

from __future__ import annotations

import argparse
import csv
import html
from pathlib import Path


WIDTH = 1280
PANEL_HEIGHT = 250
LEFT = 90
RIGHT = 30
TOP = 45
BOTTOM = 48
COLORS = ("#2563eb", "#f97316", "#16a34a", "#dc2626")


def number(row: dict[str, str], name: str) -> float:
    return float(row[name])


def bounds(series: list[list[float]]) -> tuple[float, float]:
    minimum = min(min(values) for values in series)
    maximum = max(max(values) for values in series)
    if minimum == maximum:
        padding = max(1.0, abs(minimum) * 0.1)
    else:
        padding = (maximum - minimum) * 0.08
    return minimum - padding, maximum + padding


def polyline(
    times: list[float], values: list[float], x_max: float, y_min: float, y_max: float, top: int
) -> str:
    chart_width = WIDTH - LEFT - RIGHT
    chart_height = PANEL_HEIGHT - TOP - BOTTOM
    points = []
    for time, value in zip(times, values):
        x = LEFT + chart_width * time / x_max if x_max else LEFT
        y = top + TOP + chart_height * (y_max - value) / (y_max - y_min)
        points.append(f"{x:.2f},{y:.2f}")
    return " ".join(points)


def panel(
    title: str,
    times: list[float],
    series: list[tuple[str, list[float], str]],
    panel_index: int,
    x_max: float,
) -> str:
    top = panel_index * PANEL_HEIGHT
    chart_height = PANEL_HEIGHT - TOP - BOTTOM
    y_min, y_max = bounds([values for _, values, _ in series])
    lines = [
        f'<text x="{LEFT}" y="{top + 25}" class="title">{html.escape(title)}</text>',
        f'<rect x="{LEFT}" y="{top + TOP}" width="{WIDTH - LEFT - RIGHT}" height="{chart_height}" class="plot"/>',
    ]
    for index in range(5):
        fraction = index / 4
        y = top + TOP + chart_height * fraction
        value = y_max - (y_max - y_min) * fraction
        lines.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{y:.1f}" y2="{y:.1f}" class="grid"/>')
        lines.append(f'<text x="{LEFT - 8}" y="{y + 4:.1f}" text-anchor="end" class="axis">{value:.3f}</text>')
    for index in range(6):
        fraction = index / 5
        x = LEFT + (WIDTH - LEFT - RIGHT) * fraction
        time = x_max * fraction
        lines.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="{top + TOP}" y2="{top + TOP + chart_height}" class="grid"/>')
        if panel_index == 2:
            lines.append(f'<text x="{x:.1f}" y="{top + TOP + chart_height + 22}" text-anchor="middle" class="axis">{time:.2f}</text>')
    if panel_index == 2:
        lines.append(f'<text x="{WIDTH / 2}" y="{top + PANEL_HEIGHT - 8}" text-anchor="middle" class="axis">Time (s)</text>')
    legend_x = WIDTH - RIGHT - 175
    for index, (label, values, color) in enumerate(series):
        legend_y = top + 20 + index * 18
        lines.append(f'<line x1="{legend_x}" x2="{legend_x + 18}" y1="{legend_y}" y2="{legend_y}" stroke="{color}" stroke-width="3"/>')
        lines.append(f'<text x="{legend_x + 24}" y="{legend_y + 4}" class="legend">{html.escape(label)}</text>')
        lines.append(f'<polyline points="{polyline(times, values, x_max, y_min, y_max, top)}" fill="none" stroke="{color}" stroke-width="2.2"/>')
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot MotionBridge telemetry as SVG.")
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("--output", "-o", type=Path, default=Path("motionbridge_telemetry.svg"))
    args = parser.parse_args()

    with args.csv_file.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise SystemExit("CSV has no telemetry rows.")

    times = [number(row, "time_s") for row in rows]
    x_max = max(times)
    position_ref = [number(row, "reference_position_rad") for row in rows]
    position_actual = [number(row, "actual_position_rad") for row in rows]
    velocity_ref = [number(row, "reference_velocity_rad_s") for row in rows]
    velocity_actual = [number(row, "actual_velocity_rad_s") for row in rows]
    torque = [number(row, "torque_nm") for row in rows]
    error = [number(row, "following_error_rad") for row in rows]

    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{PANEL_HEIGHT * 3}" viewBox="0 0 {WIDTH} {PANEL_HEIGHT * 3}">
<style>
  .plot {{ fill: #ffffff; stroke: #cbd5e1; }} .grid {{ stroke: #e2e8f0; stroke-width: 1; }}
  .title {{ font: 600 16px sans-serif; fill: #0f172a; }} .axis {{ font: 12px sans-serif; fill: #475569; }}
  .legend {{ font: 12px sans-serif; fill: #334155; }}
</style>
<rect width="100%" height="100%" fill="#f8fafc"/>
{panel("Position (rad)", times, [("Reference", position_ref, COLORS[0]), ("Actual", position_actual, COLORS[1])], 0, x_max)}
{panel("Velocity (rad/s)", times, [("Reference", velocity_ref, COLORS[0]), ("Actual", velocity_actual, COLORS[1])], 1, x_max)}
{panel("Torque and following error", times, [("Torque (Nm)", torque, COLORS[2]), ("Error (rad)", error, COLORS[3])], 2, x_max)}
</svg>'''
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(svg, encoding="utf-8")
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
