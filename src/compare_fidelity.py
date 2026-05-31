"""
compare_fidelity.py

Compares rendered .splat screenshots against the original plush.splat
renders at each distance (near/mid/far) using SSIM and PSNR.

Each variant is compared to the original at the same distance, so there
are three reference images:
  - plush_near.png
  - plush_mid.png
  - plush_far.png

Output: a new CSV with two added columns — ssim and psnr — appended to
the original benchmark data.
"""

import os
import warnings

import numpy as np
import pandas as pd
from PIL import Image
from skimage.metrics import peak_signal_noise_ratio as psnr
from skimage.metrics import structural_similarity as ssim


CSV_PATH    = "benchmark_results.csv"
IMAGES_DIR  = "screenshots"
OUTPUT_PATH = "benchmark_results_fidelity.csv"

ORIGINAL_STEM = "plush"  # reference splat filename without extension


def load_image(path: str) -> np.ndarray:
    """Load a PNG as an RGB uint8 numpy array."""
    img = Image.open(path).convert("RGB")
    return np.array(img)


def image_path(images_dir: str, splat_filename: str, distance: str) -> str:
    """
    Build the expected screenshot path for a given splat file and distance.
    Convention: <stem>_<distance>.png
    e.g. plush_100pct_m0.005_near.png
    """
    stem = os.path.splitext(splat_filename)[0]  # strip .splat
    return os.path.join(images_dir, f"{stem}_{distance}.png")


def compute_metrics(ref: np.ndarray, tgt: np.ndarray):
    """
    Compute SSIM and PSNR between two uint8 RGB images.

    SSIM  — Structural Similarity Index [0, 1]. Higher = more similar.
             channel_axis=2 tells skimage to treat the last axis as RGB.
    PSNR  — Peak Signal-to-Noise Ratio in dB. Higher = less noise.
             data_range=255 because images are uint8.
    """
    ssim_score = ssim(ref, tgt, channel_axis=2, data_range=255)
    psnr_score = psnr(ref, tgt, data_range=255)
    return ssim_score, psnr_score


def main():
    df = pd.read_csv(CSV_PATH)

    # Pre-load the three reference images (one per distance)
    distances = df["distance"].unique()
    references = {}
    for dist in distances:
        ref_path = image_path(IMAGES_DIR, f"{ORIGINAL_STEM}.splat", dist)
        if not os.path.exists(ref_path):
            raise FileNotFoundError(
                f"Reference image not found: {ref_path}\n"
                f"Expected the original '{ORIGINAL_STEM}.splat' render at distance '{dist}'."
            )
        references[dist] = load_image(ref_path)
        print(f"Loaded reference [{dist}]: {ref_path}  {references[dist].shape}")

    ssim_scores = []
    psnr_scores = []

    for _, row in df.iterrows():
        fname    = row["filename"]
        distance = row["distance"]
        tgt_path = image_path(IMAGES_DIR, fname, distance)

        if not os.path.exists(tgt_path):
            warnings.warn(f"Screenshot not found, skipping: {tgt_path}")
            ssim_scores.append(float("nan"))
            psnr_scores.append(float("nan"))
            continue

        ref = references[distance]
        tgt = load_image(tgt_path)

        # Resize target to reference dimensions if they somehow differ
        if ref.shape != tgt.shape:
            warnings.warn(
                f"Size mismatch for {tgt_path}: ref {ref.shape} vs tgt {tgt.shape}. "
                "Resizing target to match reference."
            )
            tgt_img = Image.fromarray(tgt).resize(
                (ref.shape[1], ref.shape[0]), Image.LANCZOS
            )
            tgt = np.array(tgt_img)

        s, p = compute_metrics(ref, tgt)
        ssim_scores.append(round(s, 6))
        psnr_scores.append(round(p, 4))

        print(f"  {fname} [{distance}]  SSIM={s:.4f}  PSNR={p:.2f} dB")

    df["ssim"] = ssim_scores
    df["psnr"] = psnr_scores

    df.to_csv(OUTPUT_PATH, index=False)
    print(f"\nSaved enriched CSV to: {OUTPUT_PATH}")


if __name__ == "__main__":
    main()