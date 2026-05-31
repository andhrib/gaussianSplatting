"""
generate_graphs.py

Four report graphs backing the LOD conclusion:
  1. Merge only    — SSIM vs splat count, all distances overlaid
  2. Prune only    — same axes
  3. Head-to-head  — near distance only, all three approaches compared
  4. LOD recommendation — FPS vs SSIM scatter with recommended variants annotated
"""

import os
import warnings
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import seaborn as sns

warnings.filterwarnings("ignore")

# ── paths ─────────────────────────────────────────────────────────────────────
CSV_PATH   = "benchmark_results_fidelity.csv"
OUTPUT_DIR = "graphs"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# ── theme ─────────────────────────────────────────────────────────────────────
sns.set_theme(style="whitegrid", context="paper", font_scale=1.3)
plt.rcParams.update({
    "figure.dpi": 150,
    "savefig.dpi": 200,
    "savefig.bbox": "tight",
    "font.family": "sans-serif",
    "axes.spines.top": False,
    "axes.spines.right": False,
})
 
DIST_ORDER   = ["near", "mid", "far"]
DIST_COLORS  = {"near": "#D85A30", "mid": "#1D9E75", "far": "#378ADD"}
DIST_MARKERS = {"near": "o",       "mid": "s",       "far": "^"}
DIST_DASHES  = {"near": (1, 0),    "mid": (4, 2),    "far": (1, 1)}
 
 
# ── load & enrich ─────────────────────────────────────────────────────────────
df = pd.read_csv(CSV_PATH)
df["psnr"] = pd.to_numeric(df["psnr"], errors="coerce")
 
def parse_merge(f):
    if "_m" in f:
        try: return float(f.split("_m")[1].replace(".splat", ""))
        except ValueError: return None
    return None
 
def parse_pct(f):
    for token in f.replace(".splat", "").split("_"):
        if token.endswith("pct"):
            try: return int(token.replace("pct", ""))
            except ValueError: pass
    return 100
 
df["merge"] = df["filename"].apply(parse_merge)
df["pct"]   = df["filename"].apply(parse_pct)
df["is_original"] = df["filename"].isin(["plush.splat", "plush_100pct.splat"])
 
merge_only  = df[df["merge"].notna() & (df["pct"] == 100) & ~df["is_original"]].copy()
prune_only  = df[df["merge"].isna()  & ~df["is_original"]].copy()
# Best-case combined: 75pct + m0.01 (most generous combination tested)
combined    = df[(df["merge"] == 0.01) & (df["pct"] == 75)].copy()
 
 
def save(fig, name):
    path = os.path.join(OUTPUT_DIR, name)
    fig.savefig(path)
    plt.close(fig)
    print(f"  saved → {path}")
 
 
# ═════════════════════════════════════════════════════════════════════════════
# 1. Merge only — SSIM vs splat count, distances overlaid
# ═════════════════════════════════════════════════════════════════════════════
print("1. Merge only: SSIM vs splat count")
fig, ax = plt.subplots(figsize=(7, 5))
for dist in DIST_ORDER:
    sub = merge_only[merge_only["distance"] == dist].sort_values("splat_count")
    ax.plot(sub["splat_count"], sub["ssim"],
            color=DIST_COLORS[dist], marker=DIST_MARKERS[dist],
            dashes=DIST_DASHES[dist], linewidth=2, markersize=7,
            label=dist, zorder=3)
 
ax.set_xlabel("splat count")
ax.set_ylabel("SSIM")
ax.set_ylim(0.55, 1.05)
ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f"{x/1000:.0f}k"))
ax.legend(title="distance")
ax.set_title("Voxel merging: fidelity vs splat count")
fig.tight_layout()
save(fig, "01_merge_only_ssim.png")
 
 
# ═════════════════════════════════════════════════════════════════════════════
# 2. Prune only — SSIM vs splat count, distances overlaid
# ═════════════════════════════════════════════════════════════════════════════
print("2. Prune only: SSIM vs splat count")
fig, ax = plt.subplots(figsize=(7, 5))
for dist in DIST_ORDER:
    sub = prune_only[prune_only["distance"] == dist].sort_values("splat_count")
    ax.plot(sub["splat_count"], sub["ssim"],
            color=DIST_COLORS[dist], marker=DIST_MARKERS[dist],
            dashes=DIST_DASHES[dist], linewidth=2, markersize=7,
            label=dist, zorder=3)
 
ax.set_xlabel("splat count")
ax.set_ylabel("SSIM")
ax.set_ylim(0.55, 1.05)
ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f"{x/1000:.0f}k"))
ax.legend(title="distance")
ax.set_title("Importance pruning: fidelity vs splat count")
fig.tight_layout()
save(fig, "02_prune_only_ssim.png")
 
 
# ═════════════════════════════════════════════════════════════════════════════
# 3. Head-to-head at near distance
#    merge only, prune only, and best-case combined (75pct + m0.01)
# ═════════════════════════════════════════════════════════════════════════════
print("3. Head-to-head at near distance")
fig, ax = plt.subplots(figsize=(7, 5))
 
approaches = [
    (merge_only,  "merge only",          "#378ADD", "o", (1, 0)),
    (prune_only,  "prune only",          "#1D9E75", "s", (4, 2)),
    (combined,    "combined (75%, m0.01\nbest case)", "#D85A30", "^", (2, 2)),
]
 
for data, label, color, marker, dashes in approaches:
    sub = data[data["distance"] == "near"].sort_values("splat_count")
    ax.plot(sub["splat_count"], sub["ssim"],
            color=color, marker=marker, dashes=dashes,
            linewidth=2, markersize=7, label=label, zorder=3)
 
ax.set_xlabel("splat count")
ax.set_ylabel("SSIM")
ax.set_ylim(0.55, 1.05)
ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f"{x/1000:.0f}k"))
ax.legend(fontsize=10)
ax.set_title("Approach comparison at near distance\n(where differences are largest)")
fig.tight_layout()
save(fig, "03_head_to_head_near.png")
 
 
# ═════════════════════════════════════════════════════════════════════════════
# 4. FPS vs SSIM — all merge-only variants, distances as lines, nodes labeled
# ═════════════════════════════════════════════════════════════════════════════
print("4. FPS vs SSIM: all merge-only variants")
 
# Label each node with its merge threshold
NODE_LABELS = {
    0.005: "m=0.005", 0.01: "m=0.01", 0.015: "m=0.015",
    0.02: "m=0.02",  0.05: "m=0.05",  0.1:   "m=0.1",
}
# Per-distance nudge to avoid label collisions: (dx, dy) in data units
LABEL_OFFSETS = {
    "near": {"0.005": (-0.015, 2),  "0.01":  (0.003, 2),  "0.015": (0.003, 2),
             "0.02":  (0.003, 2),   "0.05":  (0.003, 2),  "0.1":   (0.003, 2)},
    "mid":  {"0.005": (-0.015, 2),  "0.01":  (0.003, 2),  "0.015": (0.003, 2),
             "0.02":  (0.003, 2),   "0.05":  (0.003, 2),  "0.1":   (0.003, 2)},
    "far":  {"0.005": (-0.015, 2),  "0.01":  (0.003, 2),  "0.015": (0.003, 2),
             "0.02":  (0.003, 2),   "0.05":  (0.003, 2),  "0.1":   (0.003, 2)},
}
 
fig, ax = plt.subplots(figsize=(9, 6))
 
for dist in DIST_ORDER:
    sub = merge_only[merge_only["distance"] == dist].sort_values("merge")
    ax.plot(sub["ssim"], sub["fps"],
            color=DIST_COLORS[dist], dashes=DIST_DASHES[dist],
            linewidth=2, alpha=0.6, zorder=2)
    ax.scatter(sub["ssim"], sub["fps"],
               color=DIST_COLORS[dist], marker=DIST_MARKERS[dist],
               s=70, zorder=3, label=dist)
    for _, row in sub.iterrows():
        m_key = str(row["merge"])
        label = NODE_LABELS.get(row["merge"], f"m={row['merge']}")
        dx, dy = LABEL_OFFSETS[dist].get(m_key, (0.003, 2))
        ax.annotate(label,
                    xy=(row["ssim"], row["fps"]),
                    xytext=(row["ssim"] + dx, row["fps"] + dy),
                    fontsize=7.5, color=DIST_COLORS[dist], alpha=0.85)
 
ax.set_xlabel("SSIM (higher = more faithful)")
ax.set_ylabel("FPS")
ax.set_title("Quality vs performance: voxel merging across distances")
ax.legend(title="distance")
fig.tight_layout()
save(fig, "04_fps_vs_ssim.png")
 
 
print("\nAll graphs saved to:", OUTPUT_DIR)