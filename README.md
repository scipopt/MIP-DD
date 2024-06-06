MIP-DD - Delta-Debugging of MIP-Solvers
========================================

MIP-DD, a C++14-based software package, applies Delta Debugging, an automated approach to isolate the cause of a software failure driven by a hypothesis-trial-result loop, to mixed-integer linear programming.
The goal is to systematically reduce the size of input problems and the complexity of the solving process that exposes incorrect behavior.

The MIP-DD is guided by a fixed reference solution and consists of several modules that modify the input problem and settings while preserving the feasibility
(but not necessarily the optimality) of the reference solution. The modules apply reductions such as deleting constraints, fixing variables to their value in the
reference solution, deleting coefficients, changing settings to given target values, modifying the sides of constraints, deleting objective components, and rounding fractional numbers.
The modules are called in an iterative process similar to MIP-presolving.

# Dependencies

The external dependency that needs to be installed by the user is boost >= 1.65.

# Building

Building the bugger works with the standard cmake workflow:
```
mkdir build
cd build
cmake .. -DSOLVER=on -DSOLVER_DIR=PATH_TO_SOLVER
make
```
Here, SOLVER stands for one of the interfaced solvers for which the installation subdirectory PATH_TO_SOLVER contains the file solver-targets.cmake required to link its shared library.
Currently supported solvers:

- SOPLEX (https://soplex.zib.de/doc/html/INSTALL.php)
- SCIP (https://scipopt.org/doc/html/INSTALL.php)

It is necessary to build the solver in optimized mode since the MIP-DD is not designed to handle assertions directly in order to keep the process performant.
Nevertheless, it is usually possible to handle assertions indirectly by reformulating the solver code to return a suitable error under the negated assertion condition.
The MIP-DD will then identify the formerly failing assertion as a solver error.
Optionally, the arithmetic type used for reductions, problems, and solutions can be selected by the cmake parameter BUGGER_ARITHMETIC, which is double by default.

To run the bugger with parameters on a settings-problem-solution instance with respect to target settings, it can be invoked by
```
bin/bugger -p BUGGER_PARAMETERS -f PROBLEM -o SOLUTION -s SOLVER_SETTINGS -t TARGET_SETTINGS
```

Before running the MIP-DD we recommend to regard the following hints to obtain a reasonable workflow:
* Determine a reference solution that is as feasible as possible. To detect a suboptimality issue, the dual bound claimed by the solver must cut off this solution. For other issues, a reference solution is not required but helps to guide the process.
* Define initial limits for the solver, for example on the total number of nodes in the branching tree, so that the bug of interest is still reproducible. This way, reductions for which the bug would be reproduced beyond these limits, will be discarded. The solver interface may restrict limits automatically with respect to some variability margin in order to accelerate the process and favor easy instances.
* The initial number of batches is defined by parameter nbatches to bound the solve invocations per module. Each module determines the number of elementary modifications and then calculates the batch size to invoke the solver at most as many times as specified. Hence, the more batches, the smaller the changes in each test run. By default, it is set to 2, which initially leads to a bisection like approach suitable to quickly trying for lucky punches but might need to be increased for sensitive issues to achieve any reductions. After every bugger round, nbatches is redefined automatically in order to keep the anticipated expenditure based on the solving effort of the last failing run provided by the solver interface constant over all rounds.

For further details please refer to the PAPER (to be published).

# Parameters

Please refer to parameters.txt for the list of default parameters.

# Integrating a MIP Solver to MIP-DD

To integrate a solver into the MIP-DD's API, a new class has to be created that inherits from SolverInterface.
Methods that are not required but enable additional functionality are marked with **optional**.
This specific SolverInterface interacts with the solver by
* parsing the settings and problem files as well as loading the data into the internal data structures (`parseSettings`, `readInstance`)
* loading the internal settings and problem into the solver (`doSetup`)
* writing the internal settings and problem to files (`writeInstance`) and
* solving the instance and checking for bugs (`solve`).
  The `solve` function returns a pair<char, SolverStatus>. The signed char encodes the validity of the solving process. Negative values are reserved for solver internal errors, 0 means that no bug is detected, while positive values represent externally detected issues identifying 1 as dual fail, 2 as primal fail, and 3 as objective fail. The SolverStatus primarily serves to provide additional information about the solution status in the log for example infeasible, unbounded, optimal, or that a specific limit is reached. To suppress certain fails, the parameter `passcodes` is the vector of codes that must not be interpreted as bugs.
  The general SolverInterface already provides functions to detect dual, primal, and objective fails based on the resulting solution information to be supplied in the 'solve' function.

# Licensing

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

# How to cite

The paper is currently submitted and under review. Therefore, please cite:
```bibtex
@misc{HKG24MIPDD,
        title={MIP-DD: A Delta Debugger for Mixed Integer Programming Solvers}, 
        author={Alexander Hoen and Dominik Kamp and Ambros Gleixner},
        institution={Zuse Institute Berlin and University of Bayreuth},
        year={2024},
        eprint={2405.19770},
        archivePrefix={arXiv},
        primaryClass={math.OC},
        code={https://github.com/scipopt/MIP-DD}
  }
```

# Contributors

[Alexander Hoen](https://www.zib.de/members/hoen)  ([@alexhoen](https://github.com/alexhoen)) &mdash; developer

[Dominik Kamp](https://www.wm.uni-bayreuth.de/de/team/kamp_dominik/index.php) ([@DominikKamp](https://github.com/DominikKamp)) &mdash; developer
