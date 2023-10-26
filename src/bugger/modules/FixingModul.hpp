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
#include "bugger/interfaces/Status.hpp"

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
         return !problem.getColFlags( )[ var ].test(ColFlag::kUbInf) ||
                !problem.getColFlags( )[ var ].test(ColFlag::kLbInf) ||
                !num.isLT(problem.getLowerBounds( )[ var ], problem.getUpperBounds( )[ var ]);
      }

      void
      removeFixedCol(Problem<double> &problem, int col) {
         return;
         Objective<double> &obj = problem.getObjective( );
         const Vec<double> &lbs = problem.getLowerBounds( );
         Vec<RowActivity<double>> &activities = problem.getRowActivities( );
         ConstraintMatrix<double> &consMatrix = problem.getConstraintMatrix( );
         Vec<RowFlags> &rflags = consMatrix.getRowFlags( );
         Vec<double> &lhs = consMatrix.getLeftHandSides( );
         Vec<double> &rhs = consMatrix.getRightHandSides( );

         assert(
               num.isEq(lbs[ col ], problem.getUpperBounds( )[ col ]) && !problem.getColFlags( )[ col ]
                     .test(ColFlag::kUbInf) &&
               !problem.getColFlags( )[ col ].test(ColFlag::kLbInf));

         auto colvec = consMatrix.getColumnCoefficients(col);

         // if it is fixed to zero activities and sides do not need to be
         // updated

         // update objective offset
         if( obj.coefficients[ col ] != 0 )
         {
            obj.offset += lbs[ col ] * obj.coefficients[ col ];
            obj.coefficients[ col ] = 0;
         }


         // fixed to nonzero value, so update sides and activities
         int collen = colvec.getLength( );
         const int *colrows = colvec.getIndices( );
         const double *colvals = colvec.getValues( );

         assert( !problem.getColFlags()[col].test( ColFlag::kFixed ) );
         problem.getColFlags()[col].set( ColFlag::kFixed );

         for( int i = 0; i != collen; ++i )
         {
            int row = colrows[ i ];

            // if the row is redundant it will also be removed and does not need
            // to be updated
            if( rflags[ row ].test(RowFlag::kRedundant))
               continue;

            // subtract constant contribution from activity and sides
            double constant = lbs[ col ] * colvals[ i ];
            activities[ row ].min -= constant;
            activities[ row ].max -= constant;

            if( !rflags[ row ].test(RowFlag::kLhsInf))
               lhs[ row ] -= constant;

            if( !rflags[ row ].test(RowFlag::kRhsInf))
               rhs[ row ] -= constant;

            // due to numerics a ranged row can become an equality
            if( !rflags[ row ].test(RowFlag::kLhsInf, RowFlag::kRhsInf,
                                    RowFlag::kEquation) &&
                lhs[ row ] == rhs[ row ] )
               rflags[ row ].set(RowFlag::kEquation);
         }

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
               // TODO currently the variables are only removed from the model, but since the matrix is not shrinked they are technically still part of the problem
               //TODO check this for non zero ub/lb
               //TODO remove just updates the rhs/lhs but the entries are still present they should be ruled out with Fixed flag -> all other modules have to cast on the fixed flag
               removeFixedCol(copy, var);
               batches.push_back(var);
               ++nbatch;

               if( nbatch >= 1 && ( nbatch >= batchsize || var <= 0 ))
               {
                  ScipInterface scipInterface { };
                  // TODO pass settings to SCIP
                  scipInterface.doSetUp(copy);
                  if( scipInterface.run(msg) != Status::kSuccess )
                  {
                     copy = Problem < double > ( problem );
                     for( const auto &item: applied_vars )
                        removeFixedCol(copy, item);
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
