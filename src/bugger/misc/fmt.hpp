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

#ifndef _BUGGER_MISC_FMT_HPP_
#define _BUGGER_MISC_FMT_HPP_

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif

/* if those macros are not defined and fmt includes windows.h
 * then many macros are defined that can interfere with standard C++ code
 */
#ifndef NOMINMAX
#define NOMINMAX
#define BUGGER_DEFINED_NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define BUGGER_DEFINED_WIN32_LEAN_AND_MEAN
#endif

#ifndef NOGDI
#define NOGDI
#define BUGGER_DEFINED_NOGDI
#endif

#include "bugger/external/fmt/format.h"
#include "bugger/external/fmt/ostream.h"

#ifdef BUGGER_DEFINED_NOGDI
#undef NOGDI
#undef BUGGER_DEFINED_NOGDI
#endif

#ifdef BUGGER_DEFINED_NOMINMAX
#undef NOMINMAX
#undef BUGGER_DEFINED_NOMINMAX
#endif

#ifdef BUGGER_DEFINED_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#undef BUGGER_DEFINED_WIN32_LEAN_AND_MEAN
#endif

#endif
