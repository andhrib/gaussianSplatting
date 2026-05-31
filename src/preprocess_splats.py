import numpy as np
import os

DTYPE = np.dtype([
    ('pos',   'f4', 3),
    ('scale', 'f4', 3),
    ('color', 'u1', 4),
    ('rot',   'u1', 4)
])


def process_splats(input_path, keep_pct, use_merge=False, voxel_size=0.1):
    splats = np.fromfile(input_path, dtype=DTYPE)
    original_count = len(splats)

    # --- 1. Importance-based selection (done BEFORE merging) ---
    # Approximates Mini-Splatting's blending-weight importance (Appendix E)
    # using static per-splat attributes available in the .splat file.
    # scale values in .splat are already exp(s), so product = volume proxy.
    # Pruning first ensures importance scores are uncontaminated by merge
    # arithmetic (merging inflates opacity via max and volume via averaging,
    # which would bias importance scores upward for merged splats).
    opacity    = splats['color'][:, 3] / 255.0
    volume     = splats['scale'][:, 0] * splats['scale'][:, 1] * splats['scale'][:, 2]
    importance = opacity * volume

    keep_count = max(1, int(original_count * (keep_pct / 100.0)))
    indices    = np.argsort(importance)[::-1][:keep_count]
    splats     = splats[indices]
    print(f"  After pruning:  {len(splats):>8,} splats")

    # --- 2. Optional voxel-grid merging (done AFTER importance pruning) ---
    if use_merge:
        splats = voxel_merge(splats, voxel_size)
        print(f"  After merging:  {len(splats):>8,} splats")

    # --- 3. Build output path and save ---
    base = os.path.splitext(input_path)[0]  # strip .splat
    if use_merge:
        suffix = f"_{int(keep_pct)}pct_m{voxel_size}.splat"
    else:
        suffix = f"_{int(keep_pct)}pct.splat"
    output_path = base + suffix

    splats.tofile(output_path)
    print(f"  Saved {original_count:,} → {len(splats):,} splats "
          f"({100 * len(splats) / original_count:.1f}%)  →  {output_path}")


def voxel_merge(splats, voxel_size):
    """
    Group splats into a uniform voxel grid and merge each group into one splat.
    Averages position, scale, and RGB; takes max opacity.
    O(n log n) — no Python loop over individual splats.
    """
    voxel_coords = np.floor(splats['pos'] / voxel_size).astype(np.int32)

    # Encode (ix, iy, iz) as a single int64 key for fast unique detection
    # Shift to avoid negative indices corrupting the encoding
    offset  = voxel_coords.min(axis=0)
    shifted = voxel_coords - offset
    dims    = shifted.max(axis=0) + 1
    keys    = (shifted[:, 0].astype(np.int64) * dims[1] * dims[2]
               + shifted[:, 1].astype(np.int64) * dims[2]
               + shifted[:, 2].astype(np.int64))

    sort_order    = np.argsort(keys, kind='stable')
    sorted_keys   = keys[sort_order]
    sorted_splats = splats[sort_order]

    # Find group boundaries
    boundaries   = np.flatnonzero(np.diff(sorted_keys)) + 1
    group_starts = np.concatenate(([0], boundaries))
    group_ends   = np.concatenate((boundaries, [len(sorted_splats)]))
    n_groups     = len(group_starts)

    merged = np.empty(n_groups, dtype=DTYPE)
    for g in range(n_groups):
        cluster = sorted_splats[group_starts[g]:group_ends[g]]
        merged[g]['pos']       = cluster['pos'].mean(axis=0)
        merged[g]['scale']     = cluster['scale'].mean(axis=0)
        merged[g]['color'][:3] = cluster['color'][:, :3].mean(axis=0).astype(np.uint8)
        merged[g]['color'][3]  = cluster['color'][:, 3].max()
        merged[g]['rot']       = cluster['rot'][0]

    return merged


BASE = 'assets/plush.splat'

# Pruning only
process_splats(BASE, keep_pct=100.0)
process_splats(BASE, keep_pct=75.0)
process_splats(BASE, keep_pct=50.0)
process_splats(BASE, keep_pct=25.0)
process_splats(BASE, keep_pct=10.0)

# Merge only
process_splats(BASE, keep_pct=100.0, use_merge=True, voxel_size=0.005)
process_splats(BASE, keep_pct=100.0, use_merge=True, voxel_size=0.01)
process_splats(BASE, keep_pct=100.0, use_merge=True, voxel_size=0.02)
process_splats(BASE, keep_pct=100.0, use_merge=True, voxel_size=0.015)

# Both
process_splats(BASE, keep_pct=75.0, use_merge=True, voxel_size=0.01)
process_splats(BASE, keep_pct=50.0, use_merge=True, voxel_size=0.01)
process_splats(BASE, keep_pct=25.0, use_merge=True, voxel_size=0.01)
process_splats(BASE, keep_pct=10.0, use_merge=True, voxel_size=0.01)

process_splats(BASE, keep_pct=75.0, use_merge=True, voxel_size=0.005)
process_splats(BASE, keep_pct=50.0, use_merge=True, voxel_size=0.005)
process_splats(BASE, keep_pct=25.0, use_merge=True, voxel_size=0.005)
process_splats(BASE, keep_pct=10.0, use_merge=True, voxel_size=0.005)

process_splats(BASE, keep_pct=75.0, use_merge=True, voxel_size=0.015)
process_splats(BASE, keep_pct=50.0, use_merge=True, voxel_size=0.015)
process_splats(BASE, keep_pct=25.0, use_merge=True, voxel_size=0.015)
process_splats(BASE, keep_pct=10.0, use_merge=True, voxel_size=0.015)

process_splats(BASE, keep_pct=75.0, use_merge=True, voxel_size=0.02)
process_splats(BASE, keep_pct=50.0, use_merge=True, voxel_size=0.02)
process_splats(BASE, keep_pct=25.0, use_merge=True, voxel_size=0.02)
process_splats(BASE, keep_pct=10.0, use_merge=True, voxel_size=0.02)