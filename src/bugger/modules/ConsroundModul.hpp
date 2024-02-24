/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
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

#ifndef __BUGGER_MODULE_CONSROUND_HPP__
#define __BUGGER_MODULE_CONSROUND_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger {

   class ConsRoundModul : public BuggerModul {

   public:

      explicit ConsRoundModul(const Message& _msg, const Num<double>& _num, const BuggerParameters& _parameters,
                              std::shared_ptr<SolverFactory>& _factory) : BuggerModul(_msg, _num, _parameters, _factory) {
         this->setName("consround");
      }

   private:

      bool
      isConsroundAdmissible(const Problem<double>& problem, const int& row) const {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         bool lhsinf = problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf);
         bool rhsinf = problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf);
         double lhs = problem.getConstraintMatrix( ).getLeftHandSides( )[ row ];
         double rhs = problem.getConstraintMatrix( ).getRightHandSides( )[ row ];
         if( ( lhsinf || rhsinf || !num.isZetaEq(lhs, rhs) ) && ( ( !lhsinf && !num.isZetaIntegral(lhs) ) || ( !rhsinf && !num.isZetaIntegral(rhs) ) ) )
            return true;
         auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int index = 0; index < data.getLength( ); ++index )
            if( !num.isZetaIntegral(data.getValues( )[ index ]) )
               return true;
         return false;
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<double>& problem, Solution<double>& solution) override {

         if( solution.status == SolutionStatus::kInfeasible || solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         int batchsize = 1;

         if( parameters.nbatches > 0 )
         {
            batchsize = parameters.nbatches - 1;
            for( int i = 0; i < problem.getNRows( ); ++i )
               if( isConsroundAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<MatrixEntry<double>> applied_entries { };
         Vec<std::pair<int, double>> applied_lefts { };
         Vec<std::pair<int, double>> applied_rights { };
         Vec<MatrixEntry<double>> batches_coeff { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);
         int batch = 0;

         for( int row = 0; row < copy.getNRows( ); ++row )
         {
            if( isConsroundAdmissible(copy, row) )
            {
               admissible = true;
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);
               double lhs = num.round(copy.getConstraintMatrix( ).getLeftHandSides( )[ row ]);
               double rhs = num.round(copy.getConstraintMatrix( ).getRightHandSides( )[ row ]);
               double activity = 0.0;
               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( solution.status == SolutionStatus::kFeasible )
                  {
                     if( !num.isZetaIntegral(data.getValues( )[ index ]) )
                     {
                        double coeff = num.round(data.getValues( )[ index ]);
                        batches_coeff.emplace_back(row, data.getIndices( )[ index ], coeff);
                        activity += solution.primal[ data.getIndices( )[ index ] ] * coeff;
                     }
                     else
                        activity += solution.primal[ data.getIndices( )[ index ] ] * data.getValues( )[ index ];
                  }
                  else
                  {
                     if( !num.isZetaIntegral(data.getValues( )[ index ]) )
                        batches_coeff.emplace_back(row, data.getIndices( )[ index ], num.round(data.getValues( )[ index ]));
                  }
               }
               if( solution.status == SolutionStatus::kFeasible )
               {
                  lhs = num.min(lhs, num.epsFloor(activity));
                  rhs = num.max(rhs, num.epsCeil(activity));
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) && !num.isZetaEq(copy.getConstraintMatrix().getLeftHandSides()[ row ], lhs) )
               {
                  copy.getConstraintMatrix( ).modifyLeftHandSide(row, num, lhs);
                  batches_lhs.emplace_back(row, lhs);
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) && !num.isZetaEq(copy.getConstraintMatrix().getRightHandSides()[ row ], rhs) )
               {
                  copy.getConstraintMatrix( ).modifyRightHandSide(row, num, rhs);
                  batches_rhs.emplace_back(row, rhs);
               }
               ++batch;
            }

            if( batch >= 1 && ( batch >= batchsize || row >= copy.getNRows( ) - 1 ) )
            {
               apply_changes(copy, batches_coeff);
               if( call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  apply_changes(copy, applied_entries);
                  for( const auto &item: applied_lefts )
                     copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, item.second );
                  for( const auto &item: applied_rights )
                     copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, item.second );
               }
               else
               {
                  applied_entries.insert(applied_entries.end(), batches_coeff.begin(), batches_coeff.end());
                  applied_lefts.insert(applied_lefts.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_rights.insert(applied_rights.end(), batches_rhs.begin(), batches_rhs.end());
               }
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
               batch = 0;
            }
         }

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_entries.empty() && applied_lefts.empty() && applied_rights.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         nchgcoefs += applied_entries.size();
         nchgsides += applied_lefts.size() + applied_rights.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
