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

#ifndef _BUGGER_MISC_OPTIONS_PARSER_HPP_
#define _BUGGER_MISC_OPTIONS_PARSER_HPP_

#include "bugger/misc/fmt.hpp"
#include <boost/program_options.hpp>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace bugger {

   using namespace boost::program_options;



   struct OptionsInfo {
      std::string problem_file;
      std::string param_settings_file;
      std::string settings_file;
      std::string target_settings_file;
      std::string solution_file;
      std::vector<std::string> unparsed_options;
      bool is_complete;

      bool
      checkFiles( ) {
         if( existsFile(problem_file))
         {
            fmt::print("file {} is not valid\n", problem_file);
            return false;
         }


         if( existsFile(param_settings_file))
         {
            fmt::print("file {} is not valid\n", param_settings_file);
            return false;
         }

         if( existsFile(solution_file))
         {
            fmt::print("file {} is not valid\n", solution_file);
            return false;
         }

         if( existsFile(settings_file))
         {
            fmt::print("file {} is not valid\n", settings_file);
            return false;
         }

         if( existsFile(target_settings_file))
         {
            fmt::print("file {} is not valid\n", target_settings_file);
            return false;
         }

         return true;
      }

      bool
      existsFile(std::string &filename) const {
         return !filename.empty( ) && !std::ifstream(filename);
      }

      void
      parse(const std::vector<std::string> &opts = std::vector<std::string>( )) {
         is_complete = false;


         options_description desc(fmt::format(""));

         desc.add_options( )("file,f", value(&problem_file), "instance file");

         desc.add_options( )("parameter-settings,p",
                             value(&param_settings_file),
                             "filename for bugger parameter settings");

         desc.add_options( )("scip-parameter-settings,s",
                             value(&settings_file),
                             "filename for SCIP parameter settings");

         desc.add_options( )("target-scip-parameter-settings,t",
                             value(&target_settings_file),
                             "filename for SCIP parameter settings");

         desc.add_options( )("solution-file,o",
                             value(&solution_file),
                             "filename for solution settings or unknown/infeasible/unbounded");

//         desc.add_options( )(
//               "debug-filename,d", value(&debug_filename)->default_value(debug_filename),
//               "if not empty, current instance is written to this file before every solve (default = "") ]");
//
//
//         desc.add_options( )(
//               "tlim", value(&tlim)->default_value(tlim),
//               "bugger time limit");
//
//         desc.add_options( )(
//               "mode,m", value(&mode)->default_value(mode),
//               "selective bugger mode (-1: reproduce and reduce, 0: only reproduce, 1: only reduce)");

         if( opts.empty( ))
         {
            fmt::print("\n{}\n", desc);
            return;
         }

         variables_map vm;

         parsed_options parsed = command_line_parser(opts)
               .options(desc)
               .allow_unregistered( )
               .run( );
         store(parsed, vm);
         notify(vm);

         if( !checkFiles( ))
            return;

         unparsed_options =
               collect_unrecognized(parsed.options, exclude_positional);

         is_complete = true;
      }
   };

   OptionsInfo
   parseOptions(int argc, char *argv[]) {
      OptionsInfo optionsInfo;
      using namespace boost::program_options;
      using boost::optional;
      std::string usage =
            fmt::format("usage:\n {} [ARGUMENTS]\n", argv[ 0 ]);

      // global description.
      // will capture the command and arguments as unrecognised
      options_description global { };
      global.add_options( )("help,h", "produce help message");
      global.add_options( )("args", value<std::vector<std::string>>( ),
                            "arguments for the command");

      positional_options_description pos;
      pos.add("args", -1);

      parsed_options parsed = command_line_parser(argc, argv)
            .options(global)
            .positional(pos)
            .allow_unregistered( )
            .run( );

      variables_map vm;
      store(parsed, vm);

      if( vm.count("help") || vm.empty( ))
      {
         fmt::print("{}\n{}", usage, global);
         optionsInfo.parse( );

         return optionsInfo;
      }

      std::vector<std::string> opts =
            collect_unrecognized(parsed.options, include_positional);

      optionsInfo.parse(opts);

      return optionsInfo;
   }

} // namespace bugger

#endif
