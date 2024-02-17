Bugger - Delta-Debugging of MILP-Solvers
========================================

# Dependencies

External dependencies that need to be installed by the user are the Intel TBB >= 2020, or TBB from oneAPI runtime library and boost >= 1.65 headers.

# Building

Building the bugger with SCIP as examined solver, works with the standard cmake workflow:
```
mkdir build
cd build
cmake .. -DSCIP_DIR=PATH_TO_SCIP_BUILD_DIR
make
```

# Usage

To run the bugger with parameters on a settings-problem-solution instance with respect to target settings, it can be invoked by
```
bin/bugger -p PARAMETERS -f PROBLEM -o SOLUTION -s SETTINGS -t TARGET_SETTINGS
```

# Parameters

* [maxrounds] is the maximum number of bugger rounds or -1 for no limit (defaults to -1)

* [maxstages] is the maximum number of bugger stages or -1 for number of modules (defaults to -1)

* [nbatches] is the maximum number of batches in each module call or 0 for singletons (defaults to 0)
