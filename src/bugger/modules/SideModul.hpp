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
#include "bugger/interfaces/Status.hpp"

namespace bugger {

   class SideModul : public BuggerModul {
   public:
      SideModul( ) : BuggerModul( ) {
         this->setName("side");
      }

      bool
      initialize( ) override {
         return false;
      }

      double get_linear_activity(SparseVectorView<double> &data, Solution<double> &solution) {
         StableSum<double> sum;
         for( int i = 0; i < data.getLength( ); i++ )
            sum.add(solution.primal[data.getIndices()[i]] * data.getValues()[i]);
         return sum.get();
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

         auto res = Problem<double>(problem);
         auto copy = Problem<double>(problem);

         int batchsize = 1;
         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( problem.getRowFlags( )[ i ].test(RowFlag::kEquation))
                  ++batchsize;

            batchsize /= options.nbatches;
         }

         int nbatch = 0;

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {

            if( copy.getRowFlags( )[ row ].test(RowFlag::kEquation))
            {
               ConstraintMatrix<double> &matrix = copy.getConstraintMatrix( );
               auto data = matrix.getRowCoefficients(row);
               bool integral = TRUE;

               for( int j = 0; j < data.getLength( ); ++j )
               {
                  if( !copy.getColFlags( )[ data.getIndices( )[ j ]].test(ColFlag::kIntegral) ||
                      !num.isIntegral(data.getValues( )[ j ]))
                  {
                     integral = FALSE;
                     break;
                  }
               }
               double fixedval;
               if( !solution_exists )
               {
                  if( integral )
                     fixedval = MAX(MIN(0.0, num.feasFloor(matrix.getRightHandSides( )[ row ])),
                                    num.feasCeil(matrix.getLeftHandSides( )[ row ]));
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

               matrix.modifyLeftHandSide(row, num, fixedval );
               matrix.modifyRightHandSide(row, num, fixedval );
               nbatch++;
               //TODO: check if this is automatically converted to an equation.
            }

            if( nbatch >= 1 && ( nbatch >= batchsize || row <= 0 ))
            {
               ScipInterface scipInterface { };
               //TODO pass settings to SCIP
               scipInterface.doSetUp(copy);

               //TODO: maybe it would be better to store the reductions and apply them to the original problem so that a thrid problem does not need to be copied
               if( scipInterface.runSCIP( ) != Status::kSuccess )
                  copy = Problem<double> (res);
               else
               {
                  res = Problem<double> (copy);
                  nchgsides += nbatch;
                  result = ModulStatus::kSuccessful;
               }
               nbatch = 0;
            }
         }

         return result;
      }
   };


} // namespace bugger

#endif
