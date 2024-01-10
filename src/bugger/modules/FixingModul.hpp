/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2023             Konrad-Zuse-Zentrum                        */
/*                     fuer Informationstechnik Berlin                       */
/*                                                                           */
/* This program is free software: you can redistribute it and/or modify      */
/* it under the terms of the GNU Lesser General Public License as published  */
/* by the Free Software Foundation, either version 3 of the License, or      */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU Lesser General Public License for more details.                       */
/*                                                                           */
/* You should have received a copy of the GNU Lesser General Public License  */
/* along with this program.  If not, see <https://www.gnu.org/licenses/>.    */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef BUGGER_MODUL_FIXING_HPP_
#define BUGGER_MODUL_FIXING_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class FixingModul : public BuggerModul {
   public:
      FixingModul( const Message &_msg, const Num<double> &_num, const SolverStatus& _status) : BuggerModul( ) {
         this->setName("fixing");
         this->msg = _msg;
         this->num = _num;
         this->originalSolverStatus = _status;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isFixingAdmissible(const Problem<double>& problem, int var) {
         return !problem.getColFlags( )[ var ].test(ColFlag::kFixed)
             && !problem.getColFlags( )[ var ].test(ColFlag::kLbInf)
             && !problem.getColFlags( )[ var ].test(ColFlag::kUbInf)
             && num.isZetaEq(problem.getLowerBounds( )[ var ], problem.getUpperBounds( )[ var ]);
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution, bool solution_exists,
              const BuggerOptions &options, const Timer &timer) override {

         auto copy = Problem<double>(problem);
         Vec<int> applied_vars { };
         Vec<int> batches { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int var = problem.getNCols( ) - 1; var >= 0; --var )
               if( isFixingAdmissible(problem, var) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         batches.reserve(batchsize);
         bool admissible = false;

         for( int var = copy.getNCols( ) - 1; var >= 0; --var )
         {
            if( isFixingAdmissible(copy, var) )
            {
               admissible = true;
               assert(!copy.getColFlags( )[ var ].test(ColFlag::kFixed));
               copy.getColFlags( )[ var ].set(ColFlag::kFixed);
               batches.push_back(var);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || var <= 0 ) )
            {
               auto solver = createSolver();
               solver->doSetUp(copy,  settings, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus, settings) == BuggerStatus::kSuccess )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_vars )
                  {
                     assert(!copy.getColFlags( )[ item ].test(ColFlag::kFixed));
                     copy.getColFlags( )[ item ].set(ColFlag::kFixed);
                  }
               }
               else
                  applied_vars.insert(applied_vars.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }
         if(!admissible)
            return ModulStatus::kAdmissible;
         if( applied_vars.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            naggrvars += applied_vars.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
