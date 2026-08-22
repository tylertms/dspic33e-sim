foreach(required_variable IN
        ITEMS COVERAGE_BUILD_DIRECTORY COVERAGE_SOURCE_DIRECTORY
              COVERAGE_TARGET_PREFIX GCOV_EXECUTABLE)
  if(NOT DEFINED ${required_variable})
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

set(coverage_data)
foreach(target_suffix IN ITEMS simulator firmware_image firmware_runner)
  file(
    GLOB_RECURSE target_coverage_data
    LIST_DIRECTORIES false
    "${COVERAGE_BUILD_DIRECTORY}/CMakeFiles/${COVERAGE_TARGET_PREFIX}_${target_suffix}.dir/*.gcda"
  )
  list(APPEND coverage_data ${target_coverage_data})
endforeach()

if(NOT coverage_data)
  message(FATAL_ERROR "No simulator coverage data was generated")
endif()

list(SORT coverage_data)
execute_process(
  COMMAND
    "${GCOV_EXECUTABLE}" --branch-counts --branch-probabilities --no-output
    --source-prefix "${COVERAGE_SOURCE_DIRECTORY}" ${coverage_data}
  WORKING_DIRECTORY "${COVERAGE_BUILD_DIRECTORY}"
  RESULT_VARIABLE coverage_result
  OUTPUT_VARIABLE coverage_output
  ERROR_VARIABLE coverage_error)
if(NOT coverage_result EQUAL 0)
  message(FATAL_ERROR "gcov failed:\n${coverage_error}")
endif()

function(coverage_read_count match covered_variable total_variable)
  string(REGEX REPLACE ".*:([0-9]+)\\.([0-9][0-9])% of ([0-9]+)" "\\1;\\2;\\3"
                       values "${match}")
  list(GET values 0 whole)
  list(GET values 1 fraction)
  list(GET values 2 total)
  string(REGEX REPLACE "^0+" "" fraction "${fraction}")
  if(fraction STREQUAL "")
    set(fraction 0)
  endif()
  math(EXPR basis_points "${whole} * 100 + ${fraction}")
  math(EXPR covered "(${basis_points} * ${total} + 5000) / 10000")
  set(${covered_variable}
      "${covered}"
      PARENT_SCOPE)
  set(${total_variable}
      "${total}"
      PARENT_SCOPE)
endfunction()

function(coverage_sum_counts label covered_variable total_variable)
  string(REGEX MATCHALL "${label}:[0-9]+\\.[0-9][0-9]% of [0-9]+" matches
               "${coverage_output}")
  set(covered 0)
  set(total 0)
  foreach(match IN LISTS matches)
    coverage_read_count("${match}" match_covered match_total)
    math(EXPR covered "${covered} + ${match_covered}")
    math(EXPR total "${total} + ${match_total}")
  endforeach()
  set(${covered_variable}
      "${covered}"
      PARENT_SCOPE)
  set(${total_variable}
      "${total}"
      PARENT_SCOPE)
endfunction()

function(coverage_pad value width align output_variable)
  string(LENGTH "${value}" length)
  math(EXPR padding "${width} - ${length}")
  string(REPEAT " " ${padding} spaces)
  if(align STREQUAL "LEFT")
    set(value "${value}${spaces}")
  else()
    set(value "${spaces}${value}")
  endif()
  set(${output_variable}
      "${value}"
      PARENT_SCOPE)
endfunction()

function(coverage_print label covered total)
  if(total EQUAL 0)
    set(basis_points 10000)
  else()
    math(EXPR basis_points "(${covered} * 10000 + ${total} / 2) / ${total}")
  endif()
  math(EXPR whole "${basis_points} / 100")
  math(EXPR fraction "${basis_points} % 100")
  if(fraction LESS 10)
    set(fraction "0${fraction}")
  endif()
  coverage_pad("${label}" 12 LEFT padded_label)
  coverage_pad("${covered}" 9 RIGHT padded_covered)
  coverage_pad("${total}" 9 RIGHT padded_total)
  coverage_pad("${whole}.${fraction}%" 8 RIGHT padded_percent)
  message(
    "  ${padded_label}  ${padded_covered}  ${padded_total}  ${padded_percent}")
endfunction()

string(REGEX MATCHALL "Lines executed:[0-9]+\\.[0-9][0-9]% of [0-9]+"
             line_matches "${coverage_output}")
if(NOT line_matches)
  message(FATAL_ERROR "gcov did not report line coverage")
endif()
list(GET line_matches -1 line_match)
coverage_read_count("${line_match}" line_covered line_total)
coverage_sum_counts("Taken at least once" branch_covered branch_total)
coverage_sum_counts("Calls executed" call_covered call_total)

message("")
message("dsPIC33EP-MU simulator coverage")
message("")
message("  Metric          Covered      Total  Coverage")
message("  ------------  ---------  ---------  --------")
coverage_print("Lines" ${line_covered} ${line_total})
coverage_print("Branch paths" ${branch_covered} ${branch_total})
coverage_print("Calls" ${call_covered} ${call_total})
message("")
