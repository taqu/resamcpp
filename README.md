# resampcpp
Resampling functions for audio programming. This is forked form the [resampy](https://github.com/bmcfee/resampy).

# Prerequisites

| Name | Version |
| :--- | :---    |
| CMake | 3.18<= |

# Build
To use the Intel's ISPC compiler, pass `USE_ISPC` to the CMake.

For msvc,
```cpp
$ mkdir build & cd build
$ cmake -G"Visual Studio 16 2019" .. -DUSE_ISPC=1
```

# Usage

``` cpp
```

# WIP
At first, I try to improve processing time performance with the Intel's ISPC.
Because I don't have enough ability, the ISPC routine is super slow than the raw C++ function.

# License
You can use this software under the resampy's license, see LICENSE.
