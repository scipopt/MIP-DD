include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 14)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(bugger-executable ${CMAKE_CURRENT_LIST_DIR}/../src/bugger.cpp)
set_target_properties(bugger-executable PROPERTIES OUTPUT_NAME bugger RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(bugger-executable PRIVATE BUGGER_USE_EXTERN_TEMPLATES BUGGER_USE_EXTERN_TEMPLATES)

if(DEFINED SCIP_DIR)
    find_package(SCIP REQUIRED)
    include_directories(${SCIP_INCLUDE_DIRS})
    target_compile_definitions(bugger-executable PRIVATE BUGGER_WITH_SCIP)
    add_custom_target(bugger-symlink ALL COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_SONAME_FILE:${SCIP_LIBRARIES}> ${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:${SCIP_LIBRARIES}>)
    target_link_libraries(bugger-executable ${SCIP_LIBRARIES})
elseif(DEFINED SOPLEX_DIR)
    find_package(SOPLEX REQUIRED)
    include_directories(${SOPLEX_INCLUDE_DIRS})
    target_compile_definitions(bugger-executable PRIVATE BUGGER_WITH_SOPLEX)
    add_custom_target(bugger-symlink ALL COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_SONAME_FILE:${SOPLEX_LIBRARIES}> ${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:${SOPLEX_LIBRARIES}>)
    target_link_libraries(bugger-executable ${SOPLEX_LIBRARIES})
endif()

target_link_libraries(bugger-executable bugger-core)
