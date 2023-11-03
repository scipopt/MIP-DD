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
      FixingModul(const Message &_msg) : BuggerModul( ) {
         this->setName("fixing");
         this->msg = _msg;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isFixingAdmissible(Problem<double> &problem, int var) {
         if( problem.getColFlags( )[ var ].test(ColFlag::kFixed))
            return false;
         return !problem.getColFlags( )[ var ].test(ColFlag::kUbInf) &&
                !problem.getColFlags( )[ var ].test(ColFlag::kLbInf) &&
                num.isEq(problem.getLowerBounds( )[ var ], problem.getUpperBounds( )[ var ]);
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;
         auto copy = Problem < double > ( problem );
         Vec<int> applied_vars { };
         Vec<int> batches { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int var = problem.getNCols( ) - 1; var >= 0; --var )
               if( isFixingAdmissible(problem, var))
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         int nbatch = 0;

         for( int var = copy.getNCols( ) - 1; var >= 0; --var )
         {

            if( isFixingAdmissible(copy, var))
            {
               copy.getColFlags( )[ var ].set(ColFlag::kFixed);
               batches.push_back(var);
               ++nbatch;

               if( nbatch >= 1 && ( nbatch >= batchsize || var <= 0 ))
               {
                  auto solver = createSolver();
                  solver->parseParameters();
                  solver->doSetUp(copy, solution_exists, solution);
                  if( solver->run(msg,originalSolverStatus) != BuggerStatus::kFail )
                  {
                     copy = Problem < double > ( problem );
                     for( const auto &item: applied_vars )
                        copy.getColFlags( )[ item ].set(ColFlag::kFixed);
                  }
                  else
                  {
                     //TODO: push back together
                     for( const auto &item: batches )
                        applied_vars.push_back(item);
                     naggrvars += nbatch;
                     batches.clear( );
                     result = ModulStatus::kSuccessful;
                  }
                  nbatch = 0;
               }
               ++nbatch;
            }
         }

         problem = Problem < double > ( copy );
         return result;
      }
   };


} // namespace bugger

#endif
