Name: Delta-Bugging for SCIP
==========================================


# Dependencies

External dependencies that need to be installed by the user are the Intel TBB >= 2020, or TBB from oneAPI runtime library and boost >= 1.65 headers.
The executable additionally requires some of the boost runtime libraries that are not required when PaPILO is used as
a library.
Under the folder external/ there are additional packages that are directly included within PaPILO and have a
liberal open-source license.

If TBB is not found, then PaPILO tries to compile a static version. However this may fail on some systems currently and it is strongly recommended to install an Intel TBB runtime library.


# Building

Building NAME works with the standard cmake workflow:
(_we recommend running the make command without specifying the number of jobs
that can run simultaneously (no -j n), since this may cause large memory consumption and freeze of the machine_)

Building NAME with SCIP works also with the standard cmake workflow:
```
mkdir build
cd build
cmake -DSCIP_DIR=PATH_TO_SCIP_BUILD_DIR ..
make
```

# Usage of the binary


If Boost is not installed the Boost_Root directory has to be passed to the cmake command.

```
build/bin/bugger -f PATH_TO_INSTANCE_FILE -o PATH_TO_SOL_FILE -p PATH_TO_BUGGER_SETTINGS -s PATH_TO_SCIP_SETTINGS
```

# Parameters

* [initround] is the initial bugger round (defaults to 0)

* [initstage] is the initial bugger stage (defaults to 0)

* [nrounds] is the maximum number of bugger rounds or -1 for no limit (defaults to -1)

* [nstages] is the maximum number of bugger stages or -1 for number of included bugger modules (defaults to -1)

* [nbatches] is the maximum number of reduction batches in each bugger call or 0 for singleton batches (defaults to 0)

* [passcodes] ... is the list of non-zero exit codes to treat as pass (empty by default)

