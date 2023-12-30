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

#ifndef BUGGER_COEFFICIENT_VARIABLE_HPP_
#define BUGGER_COEFFICIENT_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger
{
   class ObjectiveModul : public BuggerModul
   {
    public:
      ObjectiveModul(const std::string& _setting, const Message& _msg, const Num<double> &_num) : BuggerModul(_setting)
      {
         this->setName( "objective" );
         this->msg = _msg;
         this->num = _num;
      }

      bool
      initialize( ) override
      {
         return false;
      }

      bool isObjectiveAdmissible(const Problem<double>& problem, int var)
      {
         return !num.isZetaZero(problem.getObjective( ).coefficients[ var ])
           && ( problem.getColFlags( )[ var ].test(ColFlag::kLbInf)
             || problem.getColFlags( )[ var ].test(ColFlag::kUbInf)
             || !num.isZetaEq(problem.getLowerBounds( )[ var ], problem.getUpperBounds( )[ var ]) );
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double>& solution, bool solution_exists, const BuggerOptions &options, const Timer &timer) override
      {
         auto copy = Problem<double>(problem);
         Vec<int> applied_reductions { };
         Vec<int> batches { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = problem.getNCols( ) - 1; i >= 0; --i )
               if( isObjectiveAdmissible(problem, i) )
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         batches.reserve(batchsize);

         for( int var = copy.getNCols( ) - 1; var >= 0; --var )
         {
            if( isObjectiveAdmissible(copy, var) )
            {
               copy.getObjective( ).coefficients[ var ] = 0.0;
               batches.push_back(var);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || var <= 0 ) )
            {
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus) == BuggerStatus::kSuccess )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
                     copy.getObjective( ).coefficients[ item ] = 0.0;
               }
               else
                  applied_reductions.insert(applied_reductions.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }

         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         else
         {
            problem = copy;
            nchgcoefs += applied_reductions.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
