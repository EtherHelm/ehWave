#!/bin/bash

# Clean OpenFOAM calculation results
echo "Cleaning previous results..."
rm -rf processor*
rm -rf postProcessing
foamListTimes -rm

# Initialize the case
echo "Initializing the case..."
blockMesh
./setWaveField

# Decompose mesh for parallel computing
echo "Decomposing mesh..."
decomposePar

# Run interFoam in parallel
echo "Running interFoam in parallel..."
mpirun --allow-run-as-root -np 32 interFoam -parallel

# Plot free surface
echo "Plotting free surface..."
python3 plot_freesurface.py
