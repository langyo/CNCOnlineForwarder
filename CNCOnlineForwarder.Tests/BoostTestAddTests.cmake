# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

set(prefix "${TEST_PREFIX}")
set(suffix "${TEST_SUFFIX}")
set(extra_args ${TEST_EXTRA_ARGS})
set(properties ${TEST_PROPERTIES})
set(script)
set(suite)
set(tests)

function(add_command NAME)
  set(_args "")
  foreach(_arg ${ARGN})
    if(_arg MATCHES "[^-./:a-zA-Z0-9_]")
      set(_args "${_args} [==[${_arg}]==]")
    else()
      set(_args "${_args} ${_arg}")
    endif()
  endforeach()
  set(script "${script}${NAME}(${_args})\n" PARENT_SCOPE)
endfunction()

# Run test executable to get list of available tests
if(NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR
    "Specified test executable does not exist.\n"
    "  Path: '${TEST_EXECUTABLE}'"
  )
endif()
execute_process(
  COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" --list_content
  WORKING_DIRECTORY "${TEST_WORKING_DIR}"
  TIMEOUT ${TEST_DISCOVERY_TIMEOUT}
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  RESULT_VARIABLE result
)
if(NOT output)
  # https://github.com/boostorg/test/issues/236
  set(output "${error}")
  set(error "")
endif()
if(NOT ${result} EQUAL 0)
  string(REPLACE "\n" "\n    " output "${output}")
  string(REPLACE "\n" "\n    " error "${error}")
  message(FATAL_ERROR
    "Error running test executable.\n"
    "  Path: '${TEST_EXECUTABLE}'\n"
    "  Result: ${result}\n"
    "  Output:\n"
    "    ${output}\n"
    "  Error:\n"
    "    ${error}\n"
  )
endif()
string(REPLACE "\n" ";" output "${output}")

# Parse output
set(line "")
set(parent_prefix "")
set(parent_enabled TRUE)
foreach(next_line IN LISTS output)
  if (NOT line MATCHES "( *)([^ *]+)(\\*)?:?(.*)")
    set(line "${next_line}")
    continue()
  endif()

  string(LENGTH "${CMAKE_MATCH_1}" indent)
  set(name "${parent_prefix}${CMAKE_MATCH_2}")
  if (parent_enabled AND CMAKE_MATCH_3 STREQUAL "*")
    set(enabled TRUE)
  else()
    set(enabled FALSE)
  endif()

  string(REGEX REPLACE "^( *)?.+$" "\\1" next_indent "${next_line}")
  string(LENGTH "${next_indent}" next_indent)
  if(next_indent GREATER indent)
    # Suite
    list(INSERT scope_name 0 "${name}")
    set(parent_prefix "${name}/")

    list(INSERT scope_enabled 0 ${enabled})
    set(parent_enabled ${enabled})
  else()
    # Test
    # add to script
    add_command(add_test
      "${prefix}${name}${suffix}"
      ${TEST_EXECUTOR}
      "${TEST_EXECUTABLE}"
      "--run_test=${name}"
      ${extra_args}
    )
    if(NOT enabled)
      add_command(set_tests_properties
        "${prefix}${name}${suffix}"
        PROPERTIES DISABLED TRUE
      )
    endif()
    add_command(set_tests_properties
      "${prefix}${name}${suffix}"
      PROPERTIES
      WORKING_DIRECTORY "${TEST_WORKING_DIR}"
      ${properties}
    )
   list(APPEND tests "${prefix}${name}${suffix}")
  endif()

  while(indent GREATER next_indent)
    list(REMOVE_AT scope_name 0)
    if(scope_name)
      list(GET scope_name 0 parent_prefix)
      set(parent_prefix "${parent_prefix}/")
    else()
      set(parent_prefix "")
    endif()

    list(REMOVE_AT scope_enabled 0)
    if(scope_enabled)
      list(GET scope_enabled 0 parent_enabled)
    else()
      set(scope_enabled TRUE)
    endif()

    math(EXPR indent "${indent} - 4")
  endwhile()

  set(line "${next_line}")
endforeach()

# Create a list of all discovered tests, which users may use to e.g. set
# properties on the tests
add_command(set ${TEST_LIST} ${tests})

# Write CTest script
file(WRITE "${CTEST_FILE}" "${script}")
