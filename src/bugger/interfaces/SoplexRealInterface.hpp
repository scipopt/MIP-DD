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

#ifndef __BUGGER_INTERFACES_SOPLEXREALINTERFACE_HPP__
#define __BUGGER_INTERFACES_SOPLEXREALINTERFACE_HPP__

#include "SoplexInterface.hpp"


namespace bugger
{
   using namespace soplex;

   template <typename REAL>
   class SoplexRealInterface : public SoplexInterface<REAL>
   {
   private:

      typedef soplex::Real SOPLEX_Real;

   public:

      explicit SoplexRealInterface(const Message& _msg, SoplexParameters& _parameters,
                                   HashMap<String, char>& _limits) : SoplexInterface<REAL>(_msg, _parameters, _limits)
      { }

      void
      doSetUp(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution) override
      {
         this->adjustment = &settings;
         this->model = &problem;
         this->reference = &solution;
         bool solution_exists = this->reference->status == SolutionStatus::kFeasible;
         int ncols = this->model->getNCols( );
         int nrows = this->model->getNRows( );
         const auto& varNames = this->model->getVariableNames( );
         const auto& consNames = this->model->getConstraintNames( );
         const auto& domains = this->model->getVariableDomains( );
         const auto& obj = this->model->getObjective( );
         const auto& consMatrix = this->model->getConstraintMatrix( );
         const auto& lhs_values = consMatrix.getLeftHandSides( );
         const auto& rhs_values = consMatrix.getRightHandSides( );
         const auto& cflags = this->model->getColFlags( );
         const auto& rflags = this->model->getRowFlags( );

         this->set_parameters( );
         SOPLEX_CALL_ABORT(this->soplex->setRealParam(SoplexParameters::OFFS, soplex::Real(obj.offset)));
         SOPLEX_CALL_ABORT(this->soplex->setIntParam(SoplexParameters::SENS, obj.sense ? SoPlex::OBJSENSE_MINIMIZE : SoPlex::OBJSENSE_MAXIMIZE));
         this->colNames.reMax(ncols);
         this->rowNames.reMax(nrows);
         this->inds.resize(ncols);
         if( solution_exists )
            this->value = this->model->getPrimalObjective(solution);
         else if( this->reference->status == SolutionStatus::kUnbounded )
            this->value = obj.sense ? -this->soplex->realParam(SoPlex::INFTY) : this->soplex->realParam(SoPlex::INFTY);
         else if( this->reference->status == SolutionStatus::kInfeasible )
            this->value = obj.sense ? this->soplex->realParam(SoPlex::INFTY) : -this->soplex->realParam(SoPlex::INFTY);

         for( int col = 0; col < ncols; ++col )
         {
            this->inds[col] = -1;
            if( cflags[col].test(ColFlag::kFixed) )
               continue;
            LPColReal var { };
            SOPLEX_Real lb = cflags[col].test(ColFlag::kLbInf)
                             ? -this->soplex->realParam(SoPlex::INFTY)
                             : SOPLEX_Real(domains.lower_bounds[col]);
            SOPLEX_Real ub = cflags[col].test(ColFlag::kUbInf)
                             ? this->soplex->realParam(SoPlex::INFTY)
                             : SOPLEX_Real(domains.upper_bounds[col]);
            assert(!cflags[col].test(ColFlag::kInactive) || lb == ub);
            var.setLower(lb);
            var.setUpper(ub);
            var.setObj(SOPLEX_Real(obj.coefficients[col]));
            this->inds[col] = this->soplex->numCols();
            this->soplex->addColReal(var);
            this->colNames.add(varNames[col].c_str());
         }

         for( int row = 0; row < nrows; ++row )
         {
            if( rflags[row].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[row].test(RowFlag::kLhsInf) || !rflags[row].test(RowFlag::kRhsInf));
            const auto& rowvec = consMatrix.getRowCoefficients(row);
            const auto& rowinds = rowvec.getIndices( );
            const auto& rowvals = rowvec.getValues( );
            int nrowcols = rowvec.getLength( );
            SOPLEX_Real lhs = rflags[row].test(RowFlag::kLhsInf)
                              ? -this->soplex->realParam(SoPlex::INFTY)
                              : SOPLEX_Real(lhs_values[row]);
            SOPLEX_Real rhs = rflags[row].test(RowFlag::kRhsInf)
                              ? this->soplex->realParam(SoPlex::INFTY)
                              : SOPLEX_Real(rhs_values[row]);
            DSVectorReal cons(nrowcols);
            for( int i = 0; i < nrowcols; ++i )
            {
               assert(!cflags[rowinds[i]].test(ColFlag::kFixed));
               assert(rowvals[i] != 0);
               cons.add(this->inds[rowinds[i]], SOPLEX_Real(rowvals[i]));
            }
            this->soplex->addRowReal(LPRowReal(lhs, cons, rhs));
            this->rowNames.add(consNames[row].c_str());
         }

         // initialize objective differences
         if( this->initial )
         {
            if( solution_exists )
            {
               const auto& doublesettings = this->adjustment->getDoubleSettings( );
               for( int index = 0; index < doublesettings.size( ); ++index )
               {
                  SoPlex::RealParam param = SoPlex::RealParam(doublesettings[index].first.back());
                  if( ( param == SoPlex::OBJLIMIT_LOWER || param == SoPlex::OBJLIMIT_UPPER ) && abs(this->soplex->realParam(param)) < this->soplex->realParam(SoPlex::INFTY) )
                  {
                     SOPLEX_CALL_ABORT(this->soplex->setRealParam(param, max(min(this->soplex->realParam(param) - soplex::Real(this->value), this->soplex->realParam(SoPlex::INFTY)), -this->soplex->realParam(SoPlex::INFTY))));
                     this->adjustment->setDoubleSettings(index, this->soplex->realParam(param));
                  }
               }
            }
            this->initial = false;
         }

         if( solution_exists )
         {
            if( abs(this->soplex->realParam(SoPlex::OBJLIMIT_LOWER)) < this->soplex->realParam(SoPlex::INFTY) )
               SOPLEX_CALL_ABORT(this->soplex->setRealParam(SoPlex::OBJLIMIT_LOWER, max(min(this->soplex->realParam(SoPlex::OBJLIMIT_LOWER) + soplex::Real(this->value), this->soplex->realParam(SoPlex::INFTY)), -this->soplex->realParam(SoPlex::INFTY))));
            if( abs(this->soplex->realParam(SoPlex::OBJLIMIT_UPPER)) < this->soplex->realParam(SoPlex::INFTY) )
               SOPLEX_CALL_ABORT(this->soplex->setRealParam(SoPlex::OBJLIMIT_UPPER, max(min(this->soplex->realParam(SoPlex::OBJLIMIT_UPPER) + soplex::Real(this->value), this->soplex->realParam(SoPlex::INFTY)), -this->soplex->realParam(SoPlex::INFTY))));
            if( this->parameters.set_dual_limit || this->parameters.set_prim_limit )
            {
               for( const auto& pair : this->limits )
               {
                  switch( pair.second )
                  {
                  case DUAL:
                     SOPLEX_CALL_ABORT(this->soplex->setRealParam(SoPlex::RealParam(pair.first.back()), soplex::Real(this->relax( this->value, obj.sense, 2 * max(this->soplex->realParam(SoPlex::FEASTOL), this->soplex->realParam(SoPlex::OPTTOL)), this->soplex->realParam(SoPlex::INFTY) ))));
                     break;
                  case PRIM:
                     SOPLEX_CALL_ABORT(this->soplex->setRealParam(SoPlex::RealParam(pair.first.back()), soplex::Real(this->value)));
                     break;
                  }
               }
            }
         }
      }

      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) override
      {
         char retcode = SPxSolver::ERROR;
         SolverStatus solverstatus = SolverStatus::kUndefinedError;

         this->soplex->setIntParam(SoplexParameters::VERB, this->msg.getVerbosityLevel() < VerbosityLevel::kDetailed ? SoPlex::VERBOSITY_ERROR : SoPlex::VERBOSITY_FULL);

         // optimize
         if( this->parameters.mode == -1 )
            retcode = this->soplex->solve();

         if( retcode > SPxSolver::NOT_INIT )
         {
            // translate solver status
            switch( retcode )
            {
               case SPxSolver::SINGULAR:
               case SPxSolver::NO_PROBLEM:
               case SPxSolver::REGULAR:
               case SPxSolver::RUNNING:
               case SPxSolver::UNKNOWN:
#if SOPLEX_APIVERSION >= 7
               case SPxSolver::OPTIMAL_UNSCALED_VIOLATIONS:
#endif
                  solverstatus = SolverStatus::kUnknown;
                  break;
               case SPxSolver::ABORT_CYCLING:
                  solverstatus = SolverStatus::kTerminate;
                  break;
               case SPxSolver::ABORT_TIME:
                  solverstatus = SolverStatus::kTimeLimit;
                  break;
               case SPxSolver::ABORT_ITER:
                  solverstatus = SolverStatus::kLimit;
                  break;
               case SPxSolver::ABORT_VALUE:
                  solverstatus = SolverStatus::kGapLimit;
                  break;
               case SPxSolver::OPTIMAL:
                  solverstatus = SolverStatus::kOptimal;
                  break;
               case SPxSolver::UNBOUNDED:
                  solverstatus = SolverStatus::kUnbounded;
                  break;
               case SPxSolver::INFEASIBLE:
                  solverstatus = SolverStatus::kInfeasible;
                  break;
               case SPxSolver::INForUNBD:
                  solverstatus = SolverStatus::kInfeasibleOrUnbounded;
                  break;
            }

            // reset return code
            retcode = SolverRetcode::OKAY;

            if( this->parameters.mode == -1 )
            {
               // retrieve enabled checks
               bool dual = this->soplex->isDualFeasible();
               bool primal = this->soplex->isPrimalFeasible();
               bool objective = this->soplex->hasSol();

               for( int passcode: passcodes )
               {
                  switch( passcode )
                  {
                  case SolverRetcode::DUALFAIL:
                     dual = false;
                     break;
                  case SolverRetcode::PRIMALFAIL:
                     primal = false;
                     break;
                  case SolverRetcode::OBJECTIVEFAIL:
                     objective = false;
                     break;
                  }
               }

               // declare primal solution
               Vec<Solution<REAL>> solution;

               // check dual by reference solution objective
               if( retcode == SolverRetcode::OKAY && dual )
                  retcode = this->check_dual_bound( REAL(this->soplex->objValueReal()), REAL(max(this->soplex->realParam(SoPlex::FEASTOL), this->soplex->realParam(SoPlex::OPTTOL))), REAL(this->soplex->realParam(SoPlex::INFTY)) );

               // check primal by generated solution values
               if( retcode == SolverRetcode::OKAY && ( primal || objective ) )
               {
                  DVectorReal sol(this->soplex->numCols());
                  DVectorReal ray(this->soplex->hasPrimalRay() ? this->soplex->numCols() : 0);
                  this->soplex->getPrimalReal(sol.get_ptr(), sol.dim());
                  this->soplex->getPrimalRayReal(ray.get_ptr(), ray.dim());
                  solution.resize(1);
                  translateSolution(sol, ray, *this->model, solution[0]);

                  if( primal )
                     retcode = this->check_primal_solution( solution, REAL(max(this->soplex->realParam(SoPlex::FEASTOL), this->soplex->realParam(SoPlex::OPTTOL))), REAL(this->soplex->realParam(SoPlex::INFTY)) );
               }

               // check objective by best solution evaluation
               if( retcode == SolverRetcode::OKAY && objective )
               {
                  if( solution.size() == 0 )
                     solution.emplace_back(SolutionStatus::kInfeasible);

                  retcode = this->check_objective_value( REAL(this->soplex->objValueReal()), solution[0], REAL(max(this->soplex->realParam(SoPlex::FEASTOL), this->soplex->realParam(SoPlex::OPTTOL))), REAL(this->soplex->realParam(SoPlex::INFTY)) );
               }
            }
         }
         else
         {
            // shift retcodes so that all errors have negative values
            retcode -= SPxSolver::NOT_INIT + 1;
         }
         // interpret certain passcodes as OKAY based on the user preferences
         for( int passcode: passcodes )
         {
            if( passcode == retcode )
            {
               retcode = SolverRetcode::OKAY;
               break;
            }
         }
         // restrict limit settings
         if( retcode != SolverRetcode::OKAY )
         {
            const auto& limitsettings = this->adjustment->getLimitSettings( );
            for( int index = 0; index < limitsettings.size( ); ++index )
            {
               if( limitsettings[index].second < 0 || limitsettings[index].second > 1 )
               {
                  double bound;
                  const char* name;
                  switch( this->limits.find(limitsettings[index].first)->second )
                  {
                  case REFI:
                     // assumes last refinement is finished
                     name = this->soplex->settings().intParam.name[SoPlex::IntParam(limitsettings[index].first.back())].data();
#if SOPLEX_APIVERSION >= 16
                     bound = ceil(max((1.0 + this->parameters.limitspace) * this->soplex->numRefinements(), 1.0));
#else
                     bound = DBL_MAX;
#endif
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case ITER:
                     // assumes last iteration is finished
                     name = this->soplex->settings().intParam.name[SoPlex::IntParam(limitsettings[index].first.back())].data();
                     bound = ceil(max((1.0 + this->parameters.limitspace) * this->soplex->numIterations(), 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case TIME:
                     // sensitive to processor speed variability
                     name = this->soplex->settings().realParam.name[SoPlex::RealParam(limitsettings[index].first.back())].data();
                     bound = ceil(max((1.0 + this->parameters.limitspace) * this->soplex->solveTime(), 1.0));
                     if( bound > LLONG_MAX )
                        continue;
                     else
                        break;
                  case DUAL:
                  case PRIM:
                  default:
                     SPX_MSG_ERROR(this->soplex->spxout << "unknown limit type\n");
                     assert(false);
                     continue;
                  }
                  if( limitsettings[index].second < 0 || bound < limitsettings[index].second )
                  {
                     this->msg.info("\t\t{} = {}\n", name, (long long)bound);
                     this->adjustment->setLimitSettings(index, bound);
                  }
               }
            }
         }
         return { retcode, solverstatus };
      }

      std::tuple<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>, boost::optional<Solution<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename, const String& solution_filename) override
      {
         auto parsed_settings = this->parseSettings(settings_filename);
         if( !parsed_settings )
            return { boost::none, boost::none, boost::none };
         SolverSettings settings { parsed_settings.get() };
         if( !this->soplex->readFile(problem_filename.c_str(), &this->rowNames, &this->colNames) )
            return { settings, boost::none, boost::none };
         ProblemBuilder<REAL> builder;

         // set objective offset
         builder.setObjOffset(this->soplex->realParam(SoplexParameters::OFFS));
         // set objective sense
         builder.setObjSense(this->soplex->intParam(SoplexParameters::SENS) == SoPlex::OBJSENSE_MINIMIZE);

         // reserve problem memory
         int ncols = this->soplex->numCols();
         int nrows = this->soplex->numRows();
         int nnz = this->soplex->numNonzeros();
         this->inds.resize(ncols);
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int col = 0; col < ncols; ++col )
         {
            this->inds[col] = col;
            SOPLEX_Real lb = this->soplex->lowerReal(col);
            SOPLEX_Real ub = this->soplex->upperReal(col);
            builder.setColLb(col, REAL(lb));
            builder.setColUb(col, REAL(ub));
            builder.setColLbInf(col, -lb >= this->soplex->realParam(SoPlex::INFTY));
            builder.setColUbInf(col, ub >= this->soplex->realParam(SoPlex::INFTY));
            builder.setObj(col, REAL(this->soplex->objReal(col)));
            builder.setColName(col, this->colNames[col]);
         }

         // set up rows
         builder.setNumRows(nrows);
         Vec<int> rowinds(ncols);
         Vec<REAL> rowvals(ncols);
         for( int row = 0; row < nrows; ++row )
         {
            SOPLEX_Real lhs = this->soplex->lhsReal(row);
            SOPLEX_Real rhs = this->soplex->rhsReal(row);
            DSVectorReal cons;
            this->soplex->getRowVectorReal(row, cons);
            for( int i = 0; i < cons.size(); ++i )
            {
               rowinds[i] = cons.index(i);
               rowvals[i] = REAL(cons.value(i));
            }
            builder.setRowLhs(row, REAL(lhs));
            builder.setRowRhs(row, REAL(rhs));
            builder.setRowLhsInf(row, -lhs >= this->soplex->realParam(SoPlex::INFTY));
            builder.setRowRhsInf(row, rhs >= this->soplex->realParam(SoPlex::INFTY));
            builder.addRowEntries(row, cons.size(), rowinds.data(), rowvals.data());
            builder.setRowName(row, this->rowNames[row]);
         }

         Problem<REAL> problem { builder.build() };
         Solution<REAL> solution { };
         //TODO: SoPlex solution reader
         if( !solution_filename.empty() )
            return { settings, problem, boost::none };

         return { settings, problem, solution };
      }

      std::tuple<bool, bool, bool>
      writeInstance(const String& filename, const bool& writesettings, const bool& writesolution) const override
      {
         bool successsettings = ( !writesettings && this->limits.size() == 0 ) || this->soplex->saveSettingsFile((filename + ".set").c_str(), true);
         bool successproblem = this->soplex->writeFile((filename + ".lp").c_str(), &this->rowNames, &this->colNames
#if SOPLEX_APIVERSION >= 15
               , nullptr, true, true
#endif
               );
         bool successsolution = true;

         //TODO: SoPlex solution setter
         if( writesolution && this->reference->status == SolutionStatus::kFeasible )
            successsolution = false;

         return { successsettings, successproblem, successsolution };
      }

      ~SoplexRealInterface( ) override = default;

   private:

      void
      translateSolution(const DVectorReal& sol, const DVectorReal& ray, const Problem<REAL>& problem, Solution<REAL>& solution) const
      {
         solution.status = ray.dim() >= 1 ? SolutionStatus::kUnbounded : SolutionStatus::kFeasible;
         solution.primal.resize(problem.getNCols());
         for( int col = 0; col < solution.primal.size(); ++col )
            solution.primal[col] = problem.getColFlags()[col].test(ColFlag::kFixed) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(sol[this->inds[col]]);
         if( ray.dim() >= 1 )
         {
            solution.ray.resize(problem.getNCols());
            for( int col = 0; col < solution.ray.size(); ++col )
               solution.ray[col] = problem.getColFlags()[col].test(ColFlag::kFixed) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(ray[this->inds[col]]);
         }
      }
   };

} // namespace bugger

#endif
