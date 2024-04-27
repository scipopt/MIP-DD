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

   template <typename REAL>
   class ConsRoundModul : public BuggerModul<REAL> {

   public:

      explicit ConsRoundModul(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                              std::shared_ptr<SolverFactory<REAL>>& _factory)
                              : BuggerModul<REAL>(_msg, _num, _parameters, _factory) {
         this->setName("consround");
      }

   private:

      bool
      isConsroundAdmissible(const Problem<REAL>& problem, const int& row) const {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         bool lhsinf = problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf);
         bool rhsinf = problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf);
         REAL lhs { problem.getConstraintMatrix( ).getLeftHandSides( )[ row ] };
         REAL rhs { problem.getConstraintMatrix( ).getRightHandSides( )[ row ] };
         if( ( lhsinf || rhsinf || !this->num.isZetaEq(lhs, rhs) ) && ( ( !lhsinf && !this->num.isZetaIntegral(lhs) ) || ( !rhsinf && !this->num.isZetaIntegral(rhs) ) ) )
            return true;
         const auto& data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int index = 0; index < data.getLength( ); ++index )
            if( !this->num.isZetaIntegral(data.getValues( )[ index ]) )
               return true;
         return false;
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override {

         if( solution.status == SolutionStatus::kInfeasible || solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int i = 0; i < problem.getNRows( ); ++i )
               if( isConsroundAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<REAL>(problem);
         auto& matrix = copy.getConstraintMatrix( );
         Vec<MatrixEntry<REAL>> applied_entries { };
         Vec<std::pair<int, REAL>> applied_lefts { };
         Vec<std::pair<int, REAL>> applied_rights { };
         Vec<MatrixEntry<REAL>> batches_coeff { };
         Vec<std::pair<int, REAL>> batches_lhs { };
         Vec<std::pair<int, REAL>> batches_rhs { };
         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);
         int batch = 0;

         for( int row = 0; row < copy.getNRows( ); ++row )
         {
            if( isConsroundAdmissible(copy, row) )
            {
               admissible = true;
               const auto& data = matrix.getRowCoefficients(row);
               REAL lhs { this->num.round(matrix.getLeftHandSides( )[ row ]) };
               REAL rhs { this->num.round(matrix.getRightHandSides( )[ row ]) };
               REAL activity { };
               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( solution.status == SolutionStatus::kFeasible )
                  {
                     if( !this->num.isZetaIntegral(data.getValues( )[ index ]) )
                     {
                        REAL coeff { this->num.round(data.getValues( )[ index ]) };
                        batches_coeff.emplace_back(row, data.getIndices( )[ index ], coeff);
                        activity += solution.primal[ data.getIndices( )[ index ] ] * coeff;
                     }
                     else
                        activity += solution.primal[ data.getIndices( )[ index ] ] * data.getValues( )[ index ];
                  }
                  else
                  {
                     if( !this->num.isZetaIntegral(data.getValues( )[ index ]) )
                        batches_coeff.emplace_back(row, data.getIndices( )[ index ], this->num.round(data.getValues( )[ index ]));
                  }
               }
               if( solution.status == SolutionStatus::kFeasible )
               {
                  lhs = this->num.min(lhs, this->num.epsFloor(activity));
                  rhs = this->num.max(rhs, this->num.epsCeil(activity));
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) && !this->num.isZetaEq(matrix.getLeftHandSides()[ row ], lhs) )
               {
                  matrix.modifyLeftHandSide(row, this->num, lhs);
                  batches_lhs.emplace_back(row, lhs);
               }
               if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) && !this->num.isZetaEq(matrix.getRightHandSides()[ row ], rhs) )
               {
                  matrix.modifyRightHandSide(row, this->num, rhs);
                  batches_rhs.emplace_back(row, rhs);
               }
               ++batch;
            }

            if( batch >= 1 && ( batch >= batchsize || row >= copy.getNRows( ) - 1 ) )
            {
               this->apply_changes(copy, batches_coeff);
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  this->apply_changes(copy, applied_entries);
                  for( const auto& item: applied_lefts )
                     matrix.modifyLeftHandSide( item.first, this->num, item.second );
                  for( const auto& item: applied_rights )
                     matrix.modifyRightHandSide( item.first, this->num, item.second );
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
         this->nchgcoefs += applied_entries.size();
         this->nchgsides += applied_lefts.size() + applied_rights.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
