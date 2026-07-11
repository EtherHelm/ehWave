#!/bin/bash
rm -rf 0
cp -r 0.orig 0

# Clean OpenFOAM calculation results
echo "Cleaning previous results..."
rm -rf processor*
rm -rf postProcessing
foamListTimes -rm

# Initialize the case
echo "Initializing the case..."
blockMesh
../../bin/setWaveField

# Decompose mesh for parallel computing
echo "Decomposing mesh..."
decomposePar

# Run interFoam in parallel
echo "Running interFoam in parallel..."
mpirun --allow-run-as-root -np 32 interFoam -parallel

# Plot free surface
echo "Plotting free surface..."
python3 plot_freesurface.py
