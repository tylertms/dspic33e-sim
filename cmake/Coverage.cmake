foreach(required_name IN
        ITEMS COVERAGE_BUILD_DIRECTORY COVERAGE_SOURCE_DIRECTORY
              COVERAGE_TARGET_PREFIX GCOV_EXECUTABLE)
  if(NOT DEFINED ${required_name})
    message(FATAL_ERROR "${required_name} is required")
  endif()
endforeach()

set(coverage_data)

foreach(target_name IN ITEMS simulator firmware_image firmware_runner)
  file(
    GLOB_RECURSE target_data
    LIST_DIRECTORIES false
    "${COVERAGE_BUILD_DIRECTORY}/CMakeFiles/${COVERAGE_TARGET_PREFIX}_${target_name}.dir/*.gcda"
  )
  list(APPEND coverage_data ${target_data})
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

function(coverage_read_count coverage_match covered_output total_output)
  string(REGEX REPLACE ".*:([0-9]+)\\.([0-9][0-9])% of ([0-9]+)" "\\1;\\2;\\3"
                       coverage_values "${coverage_match}")
  list(GET coverage_values 0 percent_whole)
  list(GET coverage_values 1 percent_fraction)
  list(GET coverage_values 2 total_count)
  string(REGEX REPLACE "^0+" "" percent_fraction "${percent_fraction}")
  if(percent_fraction STREQUAL "")
    set(percent_fraction 0)
  endif()

  math(EXPR coverage_basis_points
       "${percent_whole} * 100 + ${percent_fraction}")
  math(EXPR covered_count
       "(${coverage_basis_points} * ${total_count} + 5000) / 10000")
  set(${covered_output}
      "${covered_count}"
      PARENT_SCOPE)
  set(${total_output}
      "${total_count}"
      PARENT_SCOPE)
endfunction()

function(coverage_sum_counts metric_label covered_output total_output)
  string(REGEX MATCHALL "${metric_label}:[0-9]+\\.[0-9][0-9]% of [0-9]+"
               coverage_matches "${coverage_output}")
  set(covered_count 0)
  set(total_count 0)

  foreach(coverage_match IN LISTS coverage_matches)
    coverage_read_count("${coverage_match}" matched_covered matched_total)
    math(EXPR covered_count "${covered_count} + ${matched_covered}")
    math(EXPR total_count "${total_count} + ${matched_total}")
  endforeach()
  set(${covered_output}
      "${covered_count}"
      PARENT_SCOPE)
  set(${total_output}
      "${total_count}"
      PARENT_SCOPE)
endfunction()

function(coverage_pad text field_width alignment padded_output)
  string(LENGTH "${text}" text_length)
  math(EXPR padding_width "${field_width} - ${text_length}")
  string(REPEAT " " ${padding_width} padding_spaces)
  if(alignment STREQUAL "LEFT")
    set(text "${text}${padding_spaces}")
  else()
    set(text "${padding_spaces}${text}")
  endif()
  set(${padded_output}
      "${text}"
      PARENT_SCOPE)
endfunction()

function(coverage_print metric_label covered_count total_count)
  if(total_count EQUAL 0)
    set(coverage_basis_points 10000)
  else()
    math(EXPR coverage_basis_points
         "(${covered_count} * 10000 + ${total_count} / 2) / ${total_count}")
  endif()

  math(EXPR percent_whole "${coverage_basis_points} / 100")
  math(EXPR percent_fraction "${coverage_basis_points} % 100")
  if(percent_fraction LESS 10)
    set(percent_fraction "0${percent_fraction}")
  endif()

  coverage_pad("${metric_label}" 12 LEFT padded_label)
  coverage_pad("${covered_count}" 9 RIGHT padded_covered)
  coverage_pad("${total_count}" 9 RIGHT padded_total)
  coverage_pad("${percent_whole}.${percent_fraction}%" 8 RIGHT padded_percent)
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
