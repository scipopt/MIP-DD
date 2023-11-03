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

#ifndef BUGGER_SIDE_VARIABLE_HPP_
#define BUGGER_SIDE_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class SideModul : public BuggerModul {
   public:
      SideModul(const Message &_msg) : BuggerModul( ) {
         this->setName("side");
         this->msg = _msg;
      }

      bool
      initialize( ) override {
         return false;
      }



      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<std::pair<int, double>> batches { };

         int batchsize = 1;
         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int row = problem.getNRows( ) - 1; row >= 0; --row )
               if( isSideAdmissable(problem, row))
                  ++batchsize;

            batchsize /= options.nbatches;
         }

         int nbatch = 0;
         batches.reserve(batchsize);
         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isSideAdmissable(copy, row))
            {
               ConstraintMatrix<double> &matrix = copy.getConstraintMatrix( );
               auto data = matrix.getRowCoefficients(row);
               bool integral = true;

               for( int j = 0; j < data.getLength( ); ++j )
               {
                  if( !copy.getColFlags( )[ data.getIndices( )[ j ]].test(ColFlag::kIntegral) ||
                      !num.isIntegral(data.getValues( )[ j ]))
                  {
                     integral = false;
                     break;
                  }
               }
               double fixedval;
               if( !solution_exists )
               {
                  if( integral )
                     fixedval = MAX(MIN(0.0, num.epsFloor(matrix.getRightHandSides( )[ row ])),
                                    num.epsCeil(matrix.getLeftHandSides( )[ row ]));
                  else
                     fixedval = MAX(MIN(0.0, matrix.getRightHandSides( )[ row ]), matrix.getLeftHandSides( )[ row ]);
               }
               else
               {
                  if( integral )
                     fixedval = num.round(get_linear_activity(data, solution));
                  else
                     fixedval = get_linear_activity(data, solution);
               }

               matrix.modifyLeftHandSide(row, num, fixedval);
               matrix.modifyRightHandSide(row, num, fixedval);
               batches.emplace_back(row, fixedval);
               nbatch++;
            }

            if( nbatch >= 1 && ( nbatch >= batchsize || row <= 0 ))
            {
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg,originalSolverStatus) != BuggerStatus::kFail )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
                  {
                     copy.getConstraintMatrix( ).modifyLeftHandSide(item.first, num, item.second);
                     copy.getConstraintMatrix( ).modifyRightHandSide(item.first, num, item.second);
                  }
               }
               else
               {
                  //TODO: push back together
                  for( const auto &item: batches )
                     applied_reductions.push_back(item);
                  nchgsides += nbatch;
                  result = ModulStatus::kSuccessful;
               }
               nbatch = 0;
               batches.clear( );
               batches.reserve(batchsize);
            }
         }

         problem = Problem<double>(copy);
         return result;
      }

      bool isSideAdmissable(Problem<double> &copy, int row) const
      {
         return !copy.getRowFlags( )[ row ].test(RowFlag::kRedundant) && !copy.getRowFlags( )[ row ].test(RowFlag::kEquation);
      }
   };


} // namespace bugger

#endif
