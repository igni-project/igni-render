#!/bin/sh

# Shaders are staying in the igni-server repository until the igni init system
# is ready.

glslc main.vert -o vert.spv
glslc main.frag -o frag.spv
glslc beauty.vert -o beautyvert.spv
glslc beauty.frag -o beautyfrag.spv

