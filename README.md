# igni-render

Renderer component of the Igni platform

## Installation

### Prerequisites

* a C compiler
* [assimp](https://www.assimp.org)
* glslc
* libigni
* libdrm
* [make](https://www.gnu.org/software/make)
* [vulkan](https://www.vulkan.org)
* [GLFW](https://www.glfw.org) (optional)
* [autoconf](https://www.gnu.org/software/autoconf) (optional)
* [automake](https://www.gnu.org/software/automake) (optional)

### Instructions

1. Open up a terminal (if you haven't already) and go to the root directory
of this repository.
2. Type the commands `./configure`, `make` and `make install`.
If you're using linux with a window manager, configuring 
with `CFLAGS=-DWINDOWED=1` in the arguments is recommended.
3. Go to the ./src/shader subdirectory and run `compile.sh`.

## Using Windowed Mode

To start igni-render in windowed mode, run the `testrun` script in the
repository root directory.

