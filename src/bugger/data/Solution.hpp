/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _BUGGER_CORE_SOLUTION_HPP_
#define _BUGGER_CORE_SOLUTION_HPP_

#include "bugger/misc/Vec.hpp"

namespace bugger {

   enum class SolutionStatus {

      kInfeasible,
      kUnbounded,
      kFeasible,
      kUnknown,
   };

   template <typename REAL>
   class Solution {

   public:

      SolutionStatus status;
      Vec<REAL> primal;
      Vec<REAL> ray;

      explicit Solution( ) : status(SolutionStatus::kUnknown) { }

      Solution(const SolutionStatus& _status) : status(_status) { }

      Solution(const Vec<REAL>& _primal) : status(SolutionStatus::kFeasible), primal(_primal) { }
   };

} // namespace bugger

#endif
