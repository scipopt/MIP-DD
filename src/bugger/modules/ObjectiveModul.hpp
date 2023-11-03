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
   ObjectiveModul(const Message& _msg) : BuggerModul()
   {
      this->setName( "objective" );
      this->msg = _msg;
   }

   bool
   initialize( ) override
   {
      return false;
   }

   bool isObjectiveAdmissible(Problem<double>& problem, int var )
   {
      /* preserve restricted variables because they might be deleted anyway */
      return !problem.getColFlags()[var].test(ColFlag::kFixed)
         && !num.isZero(problem.getObjective().coefficients[var])
         && num.isLT(problem.getLowerBounds()[var], problem.getUpperBounds()[var]);
   }

   ModulStatus
   execute(Problem<double> &problem, Solution<double>& solution, bool solution_exists, const BuggerOptions &options, const Timer &timer) override
   {

      ModulStatus result = ModulStatus::kUnsuccesful;

      auto copy = Problem<double>(problem);
      Vec<std::pair<int, double>> applied_reductions{};
      Vec<std::pair<int, double>> batches{};
      int batchsize = 1;

      if( options.nbatches > 0 )
      {
         batchsize = options.nbatches - 1;
         for( int i = problem.getNCols() - 1; i >= 0; --i )
            if( isObjectiveAdmissible(problem, i) )
               ++batchsize;
         batchsize /= options.nbatches;
      }

      int nbatch = 0;


      for( int var = copy.getNCols() - 1; var >= 0; --var )
      {
         if( isObjectiveAdmissible(copy, var))
         {
            //TODO updating does not work in debug mode
//            if( iscip.exists_solution() )
//               SCIPsolUpdateVarObj(iscip.get_solution(), var, var->obj, 0);

            batches.push_back({ var, copy.getObjective( ).coefficients[ var ] });
            copy.getObjective( ).coefficients[ var ] = 0;
            ++nbatch;
            if( nbatch >= 1 && ( nbatch >= batchsize || var <= 0 ))
            {
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg) != BuggerStatus::kFail )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
                     copy.getObjective( ).coefficients[ item.first ] = item.second;
               }
               else
               {
                  //TODO: push back together
                  for( const auto &item: batches )
                     applied_reductions.push_back(item);
                  nchgcoefs += nbatch;
                  batches.clear();
                  result = ModulStatus::kSuccessful;
               }
               nbatch = 0;
            }
         }
      }
      problem = Problem<double>(copy);
      return result;
   }
};


} // namespace bugger

#endif
