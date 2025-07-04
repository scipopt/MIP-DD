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

#ifndef __BUGGER_INTERFACES_SOPLEXINTERFACE_HPP__
#define __BUGGER_INTERFACES_SOPLEXINTERFACE_HPP__

#include "soplex.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"

#define SOPLEX_CALL_ABORT(x) do { if( !(x) ) { SPX_MSG_ERROR(this->soplex->spxout << "Error in function call\n"); assert(false); } } while( false )


namespace bugger
{
   using namespace soplex;

   enum SoplexLimit : char
   {
      DUAL = 1,
      PRIM = 2,
      REFI = 3,
      ITER = 4,
      TIME = 5,
   };

   class SoplexParameters
   {
   public:

      static const SoPlex::IntParam VERB;
      static const SoPlex::IntParam SENS;
      static const SoPlex::RealParam OFFS;
      static const SoPlex::IntParam READ;
      static const SoPlex::IntParam SOLV;
      static const SoPlex::IntParam CHEC;
      static const SoPlex::IntParam SYNC;

      int arithmetic = 0;
      int mode = -1;
      double limitspace = 1.0;
      bool set_dual_limit = false;
      bool set_prim_limit = false;
      bool set_refi_limit = true;
      bool set_iter_limit = false;
      bool set_time_limit = false;
   };

   const SoPlex::IntParam SoplexParameters::VERB { SoPlex::VERBOSITY };
   const SoPlex::IntParam SoplexParameters::SENS { SoPlex::OBJSENSE };
   const SoPlex::RealParam SoplexParameters::OFFS { SoPlex::OBJ_OFFSET };
   const SoPlex::IntParam SoplexParameters::READ { SoPlex::READMODE };
   const SoPlex::IntParam SoplexParameters::SOLV { SoPlex::SOLVEMODE };
   const SoPlex::IntParam SoplexParameters::CHEC { SoPlex::CHECKMODE };
   const SoPlex::IntParam SoplexParameters::SYNC { SoPlex::SYNCMODE };

   template <typename REAL>
   class SoplexInterface : public SolverInterface<REAL>
   {
   protected:

      static bool initial;

      SoplexParameters& parameters;
      HashMap<String, char>& limits;
      SoPlex* soplex = nullptr;
      NameSet colNames { };
      NameSet rowNames { };
      Vec<int> inds { };

   public:

      SoplexInterface(const Message& _msg, SoplexParameters& _parameters, HashMap<String, char>& _limits) :
                      SolverInterface<REAL>(_msg), parameters(_parameters), limits(_limits)
      {
         soplex = new SoPlex();
      }

      void
      print_header( ) const override
      {
         soplex->printVersion();
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
         soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_DEBUG);
         bool success = filename.empty() || soplex->loadSettingsFile(filename.c_str());
         soplex->setIntParam(SoplexParameters::VERB, SoPlex::VERBOSITY_NORMAL);

         set_arithmetic( );

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
                  String name { 3, (char)(soplex->intParam(SoplexParameters::SENS) == SoPlex::OBJSENSE_MINIMIZE ? SoPlex::OBJLIMIT_UPPER : SoPlex::OBJLIMIT_LOWER) };
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
                  String name { 3, (char)(soplex->intParam(SoplexParameters::SENS) == SoPlex::OBJSENSE_MINIMIZE ? SoPlex::OBJLIMIT_LOWER : SoPlex::OBJLIMIT_UPPER) };
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

         if( !success )
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
               case REFI:
               case ITER:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
                  assert(false);
               }
            }
            else
               bool_settings.emplace_back( name, soplex->boolParam(SoPlex::BoolParam(i)) );
         }

         for( int i = 0; i < SoPlex::INTPARAM_COUNT; ++i )
         {
            switch( i )
            {
            case SoplexParameters::VERB:
            case SoplexParameters::SENS:
            case SoplexParameters::READ:
            case SoplexParameters::SOLV:
            case SoplexParameters::CHEC:
            case SoplexParameters::SYNC:
               continue;
            }
            String name { 1, (char)i };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case REFI:
               case ITER:
                  limit_settings.emplace_back( name, soplex->intParam(SoPlex::IntParam(i)) );
                  break;
               case DUAL:
               case PRIM:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
                  assert(false);
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
               case REFI:
               case ITER:
               case TIME:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
                  assert(false);
               }
            }
            else
               long_settings.emplace_back( name, soplex->randomSeed() );
         }

         for( int i = 0; i < SoPlex::REALPARAM_COUNT; ++i )
         {
            switch( i )
            {
            case SoplexParameters::OFFS:
               continue;
            }
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
                  limit_settings.emplace_back( name, min(ceil(soplex->realParam(SoPlex::RealParam(i))), soplex::Real(LLONG_MAX)) );
                  break;
               case REFI:
               case ITER:
               default:
                  SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
                  assert(false);
               }
            }
            else
               double_settings.emplace_back( name, soplex->realParam(SoPlex::RealParam(i)) );
         }

         return SolverSettings(bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings, limit_settings);
      }

      long long
      getSolvingEffort( ) const override
      {
         if( soplex->status() != SPxSolver::NOT_INIT )
            return soplex->numIterations();
         else
            return -1;
      }

      virtual
      ~SoplexInterface( ) override
      {
         delete soplex;
      }

   protected:

      void
      set_parameters( ) const
      {
         for( const auto& pair : this->adjustment->getBoolSettings( ) )
            SOPLEX_CALL_ABORT(soplex->setBoolParam(SoPlex::BoolParam(pair.first.back()), pair.second));
         for( const auto& pair : this->adjustment->getIntSettings( ) )
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoPlex::IntParam(pair.first.back()), pair.second));
         // set random seed
         for( const auto& pair : this->adjustment->getLongSettings( ) )
            soplex->setRandomSeed(pair.second);
         for( const auto& pair : this->adjustment->getDoubleSettings( ) )
            SOPLEX_CALL_ABORT(soplex->setRealParam(SoPlex::RealParam(pair.first.back()), pair.second));

         for( const auto& pair : this->adjustment->getLimitSettings( ) )
         {
            switch( limits.find(pair.first)->second )
            {
            case REFI:
            case ITER:
               SOPLEX_CALL_ABORT(soplex->setIntParam(SoPlex::IntParam(pair.first.back()), pair.second));
               break;
            case TIME:
               SOPLEX_CALL_ABORT(soplex->setRealParam(SoPlex::RealParam(pair.first.back()), pair.second));
               break;
            case DUAL:
            case PRIM:
            default:
               SPX_MSG_ERROR(soplex->spxout << "unknown limit type\n");
               assert(false);
            }
         }

         set_arithmetic( );
      }

   private:

      void
      set_arithmetic( ) const
      {
         switch( parameters.arithmetic )
         {
         case 0:
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::READ, SoPlex::READMODE_REAL));
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::SOLV, SoPlex::SOLVEMODE_REAL));
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::CHEC, SoPlex::CHECKMODE_REAL));
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::SYNC, SoPlex::SYNCMODE_ONLYREAL));
            break;
         case 1:
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::READ, SoPlex::READMODE_RATIONAL));
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::SOLV, SoPlex::SOLVEMODE_RATIONAL));
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::CHEC, SoPlex::CHECKMODE_RATIONAL));
            SOPLEX_CALL_ABORT(soplex->setIntParam(SoplexParameters::SYNC, SoPlex::SYNCMODE_AUTO));
            break;
         default:
            SPX_MSG_ERROR(soplex->spxout << "unknown solver arithmetic\n");
            assert(false);
         }
      }
   };

   template <typename REAL>
   bool SoplexInterface<REAL>::initial = true;

} // namespace bugger

#include "SoplexRealInterface.hpp"
#include "SoplexRationalInterface.hpp"


namespace bugger
{
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
         parameterset.addParameter("soplex.arithmetic", "arithmetic soplex type (0: double, 1: rational)", parameters.arithmetic, 0, 1);
         parameterset.addParameter("soplex.mode", "solve soplex mode (-1: optimize)", parameters.mode, -1, -1);
         parameterset.addParameter("soplex.limitspace", "relative margin when restricting limits or -1 for no restriction", parameters.limitspace, -1.0);
         parameterset.addParameter("soplex.setduallimit", "terminate when dual solution is better than reference solution (affecting)", parameters.set_dual_limit);
         parameterset.addParameter("soplex.setprimlimit", "terminate when prim solution is as good as reference solution (affecting)", parameters.set_prim_limit);
         parameterset.addParameter("soplex.setrefilimit", "restrict number of refinements automatically", parameters.set_refi_limit);
         parameterset.addParameter("soplex.setiterlimit", "restrict number of iterations automatically (effortbounding)", parameters.set_iter_limit);
         parameterset.addParameter("soplex.settimelimit", "restrict time automatically (unreproducible)", parameters.set_time_limit);
         // stalling number of refinements is unrestrictable because it is not monotonously increasing
      }

      std::unique_ptr<SolverInterface<REAL>>
      create_solver(const Message& msg) override
      {
         std::unique_ptr<SolverInterface<REAL>> soplex;
         switch( parameters.arithmetic )
         {
         case 0:
            if( initial && (soplex::Real)std::numeric_limits<REAL>::epsilon() > std::numeric_limits<soplex::Real>::epsilon() )
               msg.warn("Selected bugger arithmetic less precise than SoPlex real arithmetic.\n");
            soplex = std::unique_ptr<SolverInterface<REAL>>( new SoplexRealInterface<REAL>( msg, parameters, limits ) );
            break;
         case 1:
            if( initial && (soplex::Rational)std::numeric_limits<REAL>::epsilon() > std::numeric_limits<soplex::Rational>::epsilon() )
               msg.warn("Selected bugger arithmetic less precise than SoPlex rational arithmetic.\n");
            soplex = std::unique_ptr<SolverInterface<REAL>>( new SoplexRationalInterface<REAL>( msg, parameters, limits ) );
            break;
         default:
            throw std::runtime_error("unknown solver arithmetic");
         }
         if( initial )
         {
            // objective limits will be included in parseSettings() where sense is revealed
            if( parameters.limitspace < 0.0 )
            {
               parameters.set_refi_limit = false;
               parameters.set_iter_limit = false;
               parameters.set_time_limit = false;
            }
            else
            {
               if( parameters.set_refi_limit )
               {
#if SOPLEX_APIVERSION >= 16
                  String name { 1, (char)SoPlex::REFLIMIT };
#else
                  String name { 1, (char)SoPlex::INTPARAM_COUNT };
#endif
                  if( soplex->has_setting(name) )
                     limits[name] = REFI;
                  else
                  {
                     msg.info("Refinement limit disabled.\n");
                     parameters.set_refi_limit = false;
                  }
               }
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
