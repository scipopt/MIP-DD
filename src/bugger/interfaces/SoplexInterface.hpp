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

#ifndef __BUGGER_INTERFACES_SOPLEXINTERFACE_HPP__
#define __BUGGER_INTERFACES_SOPLEXINTERFACE_HPP__

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "soplex.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"


using namespace soplex;

namespace bugger
{
   enum SoplexLimit : char
   {
      DUAL = 1,
      PRIM = 2,
      ITER = 3,
      TIME = 4
   };

   class SoplexParameters
   {
   public:

      static const SoPlex::IntParam VERB;

      int arithmetic = 0;
      int mode = -1;
      double limitspace = 1.0;
      bool set_dual_limit = false;
      bool set_prim_limit = false;
      bool set_iter_limit = false;
      bool set_time_limit = false;
   };

   const SoPlex::IntParam SoplexParameters::VERB { SoPlex::VERBOSITY };

   template <typename REAL>
   class SoplexInterface : public SolverInterface<REAL>
   {
   private:

      static bool initial;

      SoplexParameters& parameters;
      HashMap<String, char>& limits;
      SoPlex* soplex;
      NameSet colNames { };
      NameSet rowNames { };
      Vec<int> inds;

   public:

      explicit SoplexInterface(const Message& _msg, SoplexParameters& _parameters, HashMap<String, char>& _limits) :
                               SolverInterface<REAL>(_msg), parameters(_parameters), limits(_limits)
      {
         soplex = new SoPlex();
         // suppress setting messages
         soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_DEBUG);
      }

      void
      print_header( ) const override
      {
         soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_NORMAL);
         soplex->printVersion();
         soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_DEBUG);
      }

      bool
      has_setting(const String& name) const override
      {
         switch( (unsigned char)name.front() )
         {
         case 0:
            return (unsigned char)name.back() < SoPlex::BOOLPARAM_COUNT;
         case 1:
            return (unsigned char)name.back() < SoPlex::INTPARAM_COUNT;
         // has random seed
         case 2:
            return (unsigned char)name.back() < 1;
         case 3:
            return (unsigned char)name.back() < SoPlex::REALPARAM_COUNT;
         default:
            return false;
         }
      }

      boost::optional<SolverSettings>
      parseSettings(const String& filename) const override
      {
         // include objective limits
         if( initial )
         {
            if( parameters.mode != -1 )
            {
               parameters.set_dual_limit = false;
               parameters.set_prim_limit = false;
            }
            else
            {
               if( parameters.set_dual_limit )
               {
                  String name { 3, (char)(soplex->intParam(SoPlex::OBJSENSE) == SoPlex::OBJSENSE_MINIMIZE ? SoPlex::OBJLIMIT_UPPER : SoPlex::OBJLIMIT_LOWER) };
                  if( has_setting(name) )
                     limits[name] = DUAL;
                  else
                  {
                     this->msg.info("Dual limit disabled.\n");
                     parameters.set_dual_limit = false;
                  }
               }
               if( parameters.set_prim_limit )
               {
                  String name { 3, (char)(soplex->intParam(SoPlex::OBJSENSE) == SoPlex::OBJSENSE_MINIMIZE ? SoPlex::OBJLIMIT_LOWER : SoPlex::OBJLIMIT_UPPER) };
                  if( has_setting(name) )
                     limits[name] = PRIM;
                  else
                  {
                     this->msg.info("Primal limit disabled.\n");
                     parameters.set_prim_limit = false;
                  }
               }
            }
         }

         if( !filename.empty() && !soplex->loadSettingsFile(filename.c_str()) )
            return boost::none;

         Vec<std::pair<String, bool>> bool_settings;
         Vec<std::pair<String, int>> int_settings;
         Vec<std::pair<String, long>> long_settings;
         Vec<std::pair<String, double>> double_settings;
         Vec<std::pair<String, char>> char_settings;
         Vec<std::pair<String, String>> string_settings;
         Vec<std::pair<String, long long>> limit_settings;

         for( int i = 0; i < SoPlex::BOOLPARAM_COUNT; ++i )
         {
            String name { 0, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
               case ITER:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               bool_settings.emplace_back( name, soplex->boolParam(SoPlex::BoolParam(i)) );
         }

         for( int i = 0; i < SoPlex::INTPARAM_COUNT; ++i )
         {
            if( i == SoplexParameters::VERB || i == SoPlex::OBJSENSE )
               continue;
            String name { 1, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case ITER:
                  limit_settings.emplace_back( name, soplex->intParam(SoPlex::IntParam(i)) );
                  break;
               case DUAL:
               case PRIM:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               int_settings.emplace_back( name, soplex->intParam(SoPlex::IntParam(i)) );
         }

         // parse random seed
         for( int i = 0; i < 1; ++i )
         {
            String name { 2, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
               case ITER:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               long_settings.emplace_back( name, soplex->randomSeed() );
         }

         for( int i = 0; i < SoPlex::REALPARAM_COUNT; ++i )
         {
            if( i == SoPlex::OBJ_OFFSET )
               continue;
            String name { 3, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
                  break;
               case TIME:
                  limit_settings.emplace_back( name, std::min(std::ceil(soplex->realParam(SoPlex::RealParam(i))), (double)LLONG_MAX) );
                  break;
               case ITER:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               }
            }
            else
               double_settings.emplace_back( name, soplex->realParam(SoPlex::RealParam(i)) );
         }

         return SolverSettings(bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings, limit_settings);
      }

      void
      doSetUp(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution) override
      {
         setup(settings, problem, solution);
      }

      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) override
      {
         char retcode = SPxSolver::ERROR;
         SolverStatus solverstatus = SolverStatus::kUndefinedError;

         soplex->setIntParam(SoplexParameters::VERB, this->msg.getVerbosityLevel() < VerbosityLevel::kDetailed ? SoPlex::VERBOSITY_ERROR : SoPlex::VERBOSITY_FULL);

         // optimize
         if( parameters.mode == -1 )
            retcode = soplex->solve();

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

            if( parameters.mode == -1 )
            {
               // retrieve enabled checks
               bool dual = true;
               bool primal = true;
               bool objective = true;

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
               if( retcode == SolverRetcode::OKAY && dual && soplex->isDualFeasible() )
                  retcode = this->check_dual_bound( soplex->objValueReal(), std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) );

               // check primal by generated solution values
               if( retcode == SolverRetcode::OKAY && ( primal && soplex->isPrimalFeasible() || objective ) )
               {
                  solution.resize(1);
                  solution[0].status = SolutionStatus::kFeasible;
                  solution[0].primal.resize(inds.size());
                  soplex->getPrimalReal(solution[0].primal.data(), solution[0].primal.size());

                  for( int col = solution[0].primal.size() - 1, var = soplex->numCols(); col >= var; --col )
                     solution[0].primal[col] = this->model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<REAL>::signaling_NaN() : solution[0].primal[--var];

                  if( soplex->hasPrimalRay() )
                  {
                     solution[0].status = SolutionStatus::kUnbounded;
                     solution[0].ray.resize(inds.size());
                     soplex->getPrimalRayReal(solution[0].ray.data(), solution[0].ray.size());

                     for( int col = solution[0].ray.size() - 1, var = soplex->numCols(); col >= var; --col )
                        solution[0].ray[col] = this->model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<REAL>::signaling_NaN() : solution[0].ray[--var];
                  }

                  if( primal )
                     retcode = this->check_primal_solution( solution, std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) );
               }

               // check objective by best solution evaluation
               if( retcode == SolverRetcode::OKAY && objective )
               {
                  if( solution.size() == 0 )
                     solution.emplace_back(SolutionStatus::kInfeasible);

                  retcode = this->check_objective_value( soplex->objValueReal(), solution[0], std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) );
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
                  switch( limits.find(limitsettings[index].first)->second )
                  {
                  case ITER:
                     // assumes last iteration is finished
                     name = soplex->settings().intParam.name[SoPlex::IntParam(limitsettings[index].first.back())].data();
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * soplex->numIterations(), 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case TIME:
                     // sensitive to processor speed variability
                     name = soplex->settings().realParam.name[SoPlex::RealParam(limitsettings[index].first.back())].data();
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * soplex->solveTime(), 1.0));
                     if( bound > LLONG_MAX )
                        continue;
                     else
                        break;
                  case DUAL:
                  case PRIM:
                  default:
                     SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
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

      long long
      getSolvingEffort( ) override
      {
         if( soplex->status() != SPxSolver::NOT_INIT )
            return soplex->numIterations();
         else
            return -1;
      }

      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename) override
      {
         auto settings = parseSettings(settings_filename);
         if( !soplex->readFile(problem_filename.c_str(), &rowNames, &colNames) )
            return { settings, boost::none };
         ProblemBuilder<REAL> builder;

         // set objective offset
         builder.setObjOffset(soplex->realParam(SoPlex::OBJ_OFFSET));
         // set objective sense
         builder.setObjSense(soplex->intParam(SoPlex::OBJSENSE) == SoPlex::OBJSENSE_MINIMIZE);

         // reserve problem memory
         int ncols = soplex->numCols();
         int nrows = soplex->numRows();
         int nnz = soplex->numNonzeros();
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int col = 0; col < ncols; ++col )
         {
            double lb = soplex->lowerReal(col);
            double ub = soplex->upperReal(col);
            builder.setColLb(col, lb);
            builder.setColUb(col, ub);
            builder.setColLbInf(col, -lb >= soplex->realParam(SoPlex::INFTY));
            builder.setColUbInf(col, ub >= soplex->realParam(SoPlex::INFTY));
            builder.setObj(col, soplex->objReal(col));
            builder.setColName(col, colNames[col]);
         }

         // set up rows
         builder.setNumRows(nrows);
         Vec<int> consinds(ncols);
         Vec<double> consvals(ncols);
         for( int row = 0; row < nrows; ++row )
         {
            double lhs = soplex->lhsReal(row);
            double rhs = soplex->rhsReal(row);
            DSVector cons;
            soplex->getRowVectorReal(row, cons);
            for( int i = 0; i < cons.size(); ++i )
            {
               consinds[i] = cons.index(i);
               consvals[i] = cons.value(i);
            }
            builder.setRowLhs(row, lhs);
            builder.setRowRhs(row, rhs);
            builder.setRowLhsInf(row, -lhs >= soplex->realParam(SoPlex::INFTY));
            builder.setRowRhsInf(row, rhs >= soplex->realParam(SoPlex::INFTY));
            builder.addRowEntries(row, cons.size(), consinds.data(), consvals.data());
            builder.setRowName(row, rowNames[row]);
         }

         return { settings, builder.build() };
      }

      bool
      writeInstance(const String& filename, const bool& writesettings) override
      {
         if( writesettings || limits.size() >= 1 )
         {
            soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_NORMAL);
            soplex->saveSettingsFile((filename + ".set").c_str(), true);
            soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_DEBUG);
         }
         return soplex->writeFile((filename + ".lp").c_str(), &rowNames, &colNames
#if SOPLEX_APIVERSION >= 15
               , nullptr, true, true
#endif
               );
      }

      ~SoplexInterface( ) override
      {
         delete soplex;
      }

   private:

      void
      setup(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution)
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
         const auto& rflags = this->model->getRowFlags( );

         set_parameters( );
         soplex->setRealParam(SoPlex::OBJ_OFFSET, obj.offset);
         soplex->setIntParam(SoPlex::OBJSENSE, obj.sense ? SoPlex::OBJSENSE_MINIMIZE : SoPlex::OBJSENSE_MAXIMIZE);
         colNames.reMax(this->model->getNCols( ));
         rowNames.reMax(this->model->getNRows( ));
         inds.resize(this->model->getNCols( ));
         if( solution_exists )
            this->value = this->get_primal_objective(solution);
         else if( this->reference->status == SolutionStatus::kUnbounded )
            this->value = obj.sense ? -soplex->realParam(SoPlex::INFTY) : soplex->realParam(SoPlex::INFTY);
         else if( this->reference->status == SolutionStatus::kInfeasible )
            this->value = obj.sense ? soplex->realParam(SoPlex::INFTY) : -soplex->realParam(SoPlex::INFTY);

         for( int col = 0; col < ncols; ++col )
         {
            if( domains.flags[col].test(ColFlag::kFixed) )
               inds[col] = -1;
            else
            {
               LPCol var { };
               double lb = domains.flags[col].test(ColFlag::kLbInf)
                           ? -soplex->realParam(SoPlex::INFTY)
                           : double(domains.lower_bounds[col]);
               double ub = domains.flags[col].test(ColFlag::kUbInf)
                           ? soplex->realParam(SoPlex::INFTY)
                           : double(domains.upper_bounds[col]);
               assert(!domains.flags[col].test(ColFlag::kInactive) || lb == ub);
               var.setLower(lb);
               var.setUpper(ub);
               var.setObj(obj.coefficients[col]);
               inds[col] = soplex->numCols();
               soplex->addColReal(var);
               colNames.add(varNames[col].c_str());
            }
         }

         for( int row = 0; row < nrows; ++row )
         {
            if( rflags[row].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[row].test(RowFlag::kLhsInf) || !rflags[row].test(RowFlag::kRhsInf));
            const auto& rowvec = consMatrix.getRowCoefficients(row);
            const auto& rowvals = rowvec.getValues( );
            const auto& rowinds = rowvec.getIndices( );
            double lhs = rflags[row].test(RowFlag::kLhsInf)
                         ? -soplex->realParam(SoPlex::INFTY)
                         : double(lhs_values[row]);
            double rhs = rflags[row].test(RowFlag::kRhsInf)
                         ? soplex->realParam(SoPlex::INFTY)
                         : double(rhs_values[row]);
            DSVector cons(rowvec.getLength( ));
            for( int i = 0; i < rowvec.getLength( ); ++i )
            {
               assert(!this->model->getColFlags( )[rowinds[i]].test(ColFlag::kFixed));
               assert(rowvals[i] != 0);
               cons.add(inds[rowinds[i]], rowvals[i]);
            }
            soplex->addRowReal(LPRow(lhs, cons, rhs));
            rowNames.add(consNames[row].c_str());
         }

         // initialize objective differences
         if( initial )
         {
            if( solution_exists )
            {
               const auto& doublesettings = this->adjustment->getDoubleSettings( );
               for( int index = 0; index < doublesettings.size( ); ++index )
               {
                  SoPlex::RealParam param = SoPlex::RealParam(doublesettings[index].first.back());
                  if( ( param == SoPlex::OBJLIMIT_LOWER || param == SoPlex::OBJLIMIT_UPPER ) && abs(soplex->realParam(param)) < soplex->realParam(SoPlex::INFTY) )
                  {
                     soplex->setRealParam(param, std::max(std::min(soplex->realParam(param) - this->value, soplex->realParam(SoPlex::INFTY)), -soplex->realParam(SoPlex::INFTY)));
                     this->adjustment->setDoubleSettings(index, soplex->realParam(param));
                  }
               }
            }
            initial = false;
         }

         if( solution_exists )
         {
            if( abs(soplex->realParam(SoPlex::OBJLIMIT_LOWER)) < soplex->realParam(SoPlex::INFTY) )
               soplex->setRealParam(SoPlex::OBJLIMIT_LOWER, std::max(std::min(soplex->realParam(SoPlex::OBJLIMIT_LOWER) + this->value, soplex->realParam(SoPlex::INFTY)), -soplex->realParam(SoPlex::INFTY)));
            if( abs(soplex->realParam(SoPlex::OBJLIMIT_UPPER)) < soplex->realParam(SoPlex::INFTY) )
               soplex->setRealParam(SoPlex::OBJLIMIT_UPPER, std::max(std::min(soplex->realParam(SoPlex::OBJLIMIT_UPPER) + this->value, soplex->realParam(SoPlex::INFTY)), -soplex->realParam(SoPlex::INFTY)));
            if( parameters.set_dual_limit || parameters.set_prim_limit )
            {
               for( const auto& pair : limits )
               {
                  switch( pair.second )
                  {
                  case DUAL:
                     soplex->setRealParam(SoPlex::RealParam(pair.first.back()), this->relax( this->value, obj.sense, 2 * std::max(soplex->realParam(SoPlex::FEASTOL), soplex->realParam(SoPlex::OPTTOL)), soplex->realParam(SoPlex::INFTY) ));
                     break;
                  case PRIM:
                     soplex->setRealParam(SoPlex::RealParam(pair.first.back()), this->value);
                     break;
                  }
               }
            }
         }
      }

      void
      set_parameters( ) const
      {
         for( const auto& pair : this->adjustment->getBoolSettings( ) )
            soplex->setBoolParam(SoPlex::BoolParam(pair.first.back()), pair.second);
         for( const auto& pair : this->adjustment->getIntSettings( ) )
            soplex->setIntParam(SoPlex::IntParam(pair.first.back()), pair.second);
         // set random seed
         for( const auto& pair : this->adjustment->getLongSettings( ) )
            soplex->setRandomSeed(pair.second);
         for( const auto& pair : this->adjustment->getDoubleSettings( ) )
            soplex->setRealParam(SoPlex::RealParam(pair.first.back()), pair.second);
         for( const auto& pair : this->adjustment->getLimitSettings( ) )
         {
            switch( limits.find(pair.first)->second )
            {
            case ITER:
               soplex->setIntParam(SoPlex::IntParam(pair.first.back()), pair.second);
               break;
            case TIME:
               soplex->setRealParam(SoPlex::RealParam(pair.first.back()), pair.second);
               break;
            case DUAL:
            case PRIM:
            default:
               SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
            }
         }
      }
   };

   template <typename REAL>
   bool SoplexInterface<REAL>::initial = true;

   template <typename REAL>
   class SoplexFactory : public SolverFactory<REAL>
   {
   private:

      SoplexParameters parameters { };
      HashMap<String, char> limits { };
      bool initial = true;

   public:

      void
      addParameters(ParameterSet& parameterset) override
      {
         parameterset.addParameter("soplex.arithmetic", "arithmetic soplex type (0: double)", parameters.arithmetic, 0, 0);
         parameterset.addParameter("soplex.mode", "solve soplex mode (-1: optimize)", parameters.mode, -1, -1);
         parameterset.addParameter("soplex.limitspace", "relative margin when restricting limits or -1 for no restriction", parameters.limitspace, -1.0);
         parameterset.addParameter("soplex.setduallimit", "terminate when dual solution is better than reference solution (affecting)", parameters.set_dual_limit);
         parameterset.addParameter("soplex.setprimlimit", "terminate when prim solution is as good as reference solution (affecting)", parameters.set_prim_limit);
         parameterset.addParameter("soplex.setiterlimit", "restrict number of iterations automatically (effortbounding)", parameters.set_iter_limit);
         parameterset.addParameter("soplex.settimelimit", "restrict time automatically (unreproducible)", parameters.set_time_limit);
         //TODO: Restrict monotonous limits by default
      }

      std::unique_ptr<SolverInterface<REAL>>
      create_solver(const Message& msg) override
      {
         auto soplex = std::unique_ptr<SolverInterface<REAL>>( new SoplexInterface<REAL>( msg, parameters, limits ) );
         if( initial )
         {
            // objective limits will be included in parseSettings() where sense is revealed
            if( parameters.limitspace < 0.0 )
            {
               parameters.set_iter_limit = false;
               parameters.set_time_limit = false;
            }
            else
            {
               if( parameters.set_iter_limit )
               {
                  String name { 1, (char)SoPlex::ITERLIMIT };
                  if( soplex->has_setting(name) )
                     limits[name] = ITER;
                  else
                  {
                     msg.info("Iteration limit disabled.\n");
                     parameters.set_iter_limit = false;
                  }
               }
               if( parameters.set_time_limit )
               {
                  String name { 3, (char)SoPlex::TIMELIMIT };
                  if( soplex->has_setting(name) )
                     limits[name] = TIME;
                  else
                  {
                     msg.info("Time limit disabled.\n");
                     parameters.set_time_limit = false;
                  }
               }
            }
            initial = false;
         }
         return soplex;
      }
   };

   template <typename REAL>
   std::shared_ptr<SolverFactory<REAL>>
   load_solver_factory( )
   {
      return std::shared_ptr<SolverFactory<REAL>>( new SoplexFactory<REAL>( ) );
   }

} // namespace bugger

#endif
