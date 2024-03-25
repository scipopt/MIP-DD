include(GenerateExportHeader)

find_package(SCIP REQUIRED)
set(BUGGER_HAVE_SCIP 1)

set(CMAKE_CXX_STANDARD 14)

include_directories(${SCIP_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
find_package(Boost REQUIRED iostreams serialization program_options)

set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${Boost_LIBRARIES})
check_cxx_source_compiles(
   "#include <boost/iostreams/filter/gzip.hpp>
    int main() { auto decomp = boost::iostreams::gzip_decompressor(); (void)decomp; return 0; }"
   BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB )
check_cxx_source_compiles(
   "#include <boost/iostreams/filter/bzip2.hpp>
    int main() { auto decomp = boost::iostreams::bzip2_decompressor(); (void)decomp; return 0; }"
   BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2 )


add_executable(bugger-executable ${CMAKE_CURRENT_LIST_DIR}/../src/bugger.cpp)

if(BUGGER_HAVE_SCIP)
   set(BUGGER_HAVE_SCIP 1 PARENT_SCOPE)
   target_compile_definitions(bugger-executable PRIVATE BUGGER_HAVE_SCIP)
endif()
set_target_properties(bugger-executable PROPERTIES OUTPUT_NAME bugger RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )
target_link_libraries(bugger-executable bugger-core  ${SCIP_LIBRARIES}  ${Boost_LIBRARIES})
target_compile_definitions(bugger-executable PRIVATE BUGGER_USE_EXTERN_TEMPLATES BUGGER_USE_EXTERN_TEMPLATES)

