import xml.etree.ElementTree as ET
import base64
import struct
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os

def read_vtp_points(filepath):
    tree = ET.parse(filepath)
    data_array = tree.getroot().find('.//Points/DataArray')
    b64_data = data_array.text.strip()
    decoded = base64.b64decode(b64_data)
    data_size = struct.unpack_from('Q', decoded, 0)[0]
    raw_data = decoded[8:8+data_size]
    floats = struct.unpack(f'{data_size // 4}f', raw_data)
    ncomp = int(data_array.get('NumberOfComponents', '3'))
    return np.array(floats).reshape(-1, ncomp)

surf_dir = './postProcessing/freeSurfaceSurf'
times_to_plot = [i*0.8 for i in range(1, 21)]  # Plot every second from 0 to 10 seconds
# times_to_plot = [i*0.05 for i in range(0, 8)]  # Plot every second from 0 to 10 seconds
n_subplots = len(times_to_plot)
height_per_subplot = 2.5

fig, axes = plt.subplots(n_subplots, 1, figsize=(8, height_per_subplot * n_subplots), sharex=True)

for idx, target_t in enumerate(times_to_plot):
    t_dir = None
    for d in sorted(os.listdir(surf_dir), key=float):
        if abs(float(d) - target_t) < 0.02:
            t_dir = d
            break
    if t_dir is None:
        axes[idx].text(0.5, 0.5, f'No data near t={target_t:.1f}s',
                       ha='center', va='center', transform=axes[idx].transAxes)
        continue

    vtp_file = f'{surf_dir}/{t_dir}/isoSurface.vtp'
    pts = read_vtp_points(vtp_file)
    x = pts[:, 0]
    z = pts[:, 2]

    # Average z values at duplicate x positions for a clean single-valued line
    # (2D mesh has 1 cell in y, so each x has 2 points from y=0 and y=0.04 faces)
    unique_x, inv_idx = np.unique(np.round(x, 6), return_inverse=True)
    z_avg = np.array([z[inv_idx == i].mean() for i in range(len(unique_x))])
    xs = unique_x
    zs = z_avg

    ax = axes[idx]
    ax.plot(xs, zs, '-', color='royalblue', linewidth=1.5)
    ax.fill_between(xs, zs, -0.02, alpha=0.25, color='lightblue')
    ax.set_ylabel('z (m)')
    ax.set_title(f't = {target_t:.1f} s')
    ax.set_xlim(0, 6)
    ax.set_ylim(-0.04, 0.04)
    ax.grid(True, alpha=0.3)
    ax.axhline(y=0, color='gray', linestyle='--', linewidth=0.5, alpha=0.7)
    if len(zs) > 0:
        ax.text(5.2, 0.085, f'max= {zs.max():.3f}m  min={zs.min():.3f}m',
                fontsize=7, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

axes[-1].set_xlabel('x (m)')
fig.suptitle('Free Surface Elevation (alpha.water = 0.5)', fontsize=13, y=1.01)
plt.tight_layout()
plt.savefig('./free_surface_spatial.png', dpi=150, bbox_inches='tight')
print('Saved: free_surface_spatial.png')
