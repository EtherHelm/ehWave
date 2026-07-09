import xml.etree.ElementTree as ET
import base64
import struct
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
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
times_to_plot = [i*0.8 for i in range(1, 11)]
n_subplots = len(times_to_plot)
height_per_subplot = 2.5

# 3D surface plot at selected times
times_3d = [i*0.8 for i in range(1, 11)]
n_3d = len(times_3d)
fig3d, axes3d = plt.subplots(1, n_3d, figsize=(5*n_3d, 4.5), subplot_kw={'projection': '3d'})
if n_3d == 1:
    axes3d = [axes3d]

for idx, target_t in enumerate(times_3d):
    t_dir = None
    for d in sorted(os.listdir(surf_dir), key=float):
        if abs(float(d) - target_t) < 0.02:
            t_dir = d
            break
    if t_dir is None:
        continue
    vtp_file = f'{surf_dir}/{t_dir}/isoSurface.vtp'
    pts = read_vtp_points(vtp_file)
    x = pts[:, 0]
    y = pts[:, 1]
    z = pts[:, 2]

    ax = axes3d[idx]
    sc = ax.scatter(x, y, z, c=z, cmap='RdBu_r', s=1, alpha=0.8)
    ax.set_xlabel('x (m)')
    ax.set_ylabel('y (m)')
    ax.set_zlabel('z (m)')
    ax.set_title(f't = {target_t:.1f} s')
    ax.set_xlim(0, 4)
    ax.set_ylim(0, 4)
    fig3d.colorbar(sc, ax=ax, label='z (m)', shrink=0.6)

fig3d.suptitle('3D Free Surface (alpha.water = 0.5)', fontsize=13)
plt.tight_layout()
plt.savefig('./free_surface_3d.png', dpi=150, bbox_inches='tight')
print('Saved: free_surface_3d.png')
