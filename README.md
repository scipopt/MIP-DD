MIP-Bugger - Delta-Debugging of MILP-Solvers
========================================

MIP-Bugger, a C++14-based software package, applies Delta Debugging, an automated approach to isolate the cause of a software failure driven by a hypothesis-trial-result loop, to mixed-integer linear programming.
The goal is to systematically reduce the size of input problems and the complexity of the solving process that expose incorrect behaviour.

The MIP-Bugger is guided by a fixed reference solution and consists of several modules that modify the input problem and settings while preserving the feasibility
(but not necessarily the optimality) of the reference solution. The modules apply reductions such as deleting constraints, fixing variables to their value in the
reference solution, deleting coefficients, changing settings to given target values, modifying the sides of constraints, deleting objective components, and rounding fractional numbers.
The modules are called in an iterative process similar to MIP-presolving.

# Dependencies

External dependency that need to be installed by the user is boost >= 1.65.

# Building

Building the bugger with SCIP as examined solver, works with the standard cmake workflow:
```
mkdir build
cd build
cmake .. -DSCIP_DIR=PATH_TO_SCIP_BUILD_DIR
make
```
It is necessary to build the solver in optimized mode since the MIP-Bugger is not designed to handle assertions in order to keep the process performant.
Nevertheless, it is usually easily possible to handle assertion fails by reformulating the solver to return a suitable error code under the negated assertion condition.
The MIP-Bugger will then identify the formerly failing assertion as solver error.
For information on building SCIP please refer to README.md inside the source packages available at https://scipopt.org/index.php#download.

# Usage

To run the bugger with parameters on a settings-problem-solution instance with respect to target settings, it can be invoked by
```
bin/bugger -p BUGGER_PARAMETERS -f PROBLEM -o SOLUTION -s SOLVER_SETTINGS -t TARGET_SETTINGS
```

Before running the MIP-Bugger we recommend to regard the following hints to obtain a reasonable workflow:
* Determine a reference solution that is as feasible as possible. To detect a suboptimality issue, the dual bound claimed by the solver must cut off this solution. For other issues, a reference solution is not required. 
* Add reasonable limits within the bug is reproduced to the original and target settings. This way, reductions for which the bug would be reproduced beyond the limits will be blocked. Usually, this accelerates the process and favours easy instances.
* Define the number of bugger batches. The more batches, the smaller the changes in each test run, and the more likely the bug is maintained. In practice, it should be set to increasing values for decreasing runtimes until the bug is reproduced.

For example please refer to the PAPER (To be published) or to the example folder.

# Parameters

Please refer to parameters.txt for the list of default parameters.

# Integrate a MIP-Solver to MIP-Bugger

To integrate a solver into the MIP-Bugger's API, a new class has to be created that inherits SolverInterface.
This SolverInterface interacts with the solver by
* parsing the settings and problem files as well as loading the data into the internal data structures (`parseSettings`, `readInstance`)
* translating and loading the internal settings and problem to the solver (`doSetup`)
* writing the internal settings and problem to files (`writeInstance`) and
* solving the problem and checking for bugs (`solve`). The `solve` function returns a pair<char,SolverStatus>. The char is used to encode if the instance could be solved correctly. Negative values are reserved for solver internal erros while 0 means that the problem could be solved correctly, 1 represents a dualfail, 2 a primal fail and 3 an objective fail. The SolverStatus is primarily used to be displayed in the log to provide more information and holds the solve-status for example infeasible, unbounded or timelimit. To deactivate certain passcodes/fail the function has the parameter `passcodes` indicating which char-error codes should be ignored.
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

For other licensing options we refer to https://scipopt.org/index.php#license.

# How to cite

The paper will be published soon.

# Contributors

[Alexander Hoen](https://www.zib.de/members/hoen)  ([@alexhoen](https://github.com/alexhoen)) &mdash; developer

[Dominik Kamp](https://www.wm.uni-bayreuth.de/de/team/kamp_dominik/index.php) ([@DominikKamp](https://github.com/DominikKamp)) &mdash; developer
