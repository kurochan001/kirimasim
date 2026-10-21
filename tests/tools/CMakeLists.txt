# The judging tools must fail when they should (TEST_SUITE.md).
add_test(NAME tools/negative_controls
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_tool_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(tools/negative_controls PROPERTIES LABELS Tools)

# Generated tests must match their generator (TEST_SUITE.md).
add_test(NAME tools/gen_net_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_net_matrix.py --check)
set_tests_properties(tools/gen_net_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_sign_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_sign_matrix.py --check)
set_tests_properties(tools/gen_sign_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_assign_op_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_assign_op_matrix.py --check)
set_tests_properties(tools/gen_assign_op_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_resolution_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_resolution_matrix.py --check)
set_tests_properties(tools/gen_resolution_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_violation_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_violation_matrix.py --check)
set_tests_properties(tools/gen_violation_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_range_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_range_matrix.py --check)
set_tests_properties(tools/gen_range_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_select_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_select_matrix.py --check)
set_tests_properties(tools/gen_select_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_edge_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_edge_matrix.py --check)
set_tests_properties(tools/gen_edge_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_event_expression_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_event_expression_matrix.py --check)
set_tests_properties(tools/gen_event_expression_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_first_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_first_call_matrix.py --check)
set_tests_properties(tools/gen_first_call_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_iff_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_iff_matrix.py --check)
set_tests_properties(tools/gen_iff_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_package_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_package_call_matrix.py --check)
set_tests_properties(tools/gen_package_call_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_stream_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_stream_matrix.py --check)
set_tests_properties(tools/gen_stream_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_string_conversion_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_string_conversion_matrix.py --check)
set_tests_properties(tools/gen_string_conversion_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_ref_wait_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_ref_wait_matrix.py --check)
set_tests_properties(tools/gen_ref_wait_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_static_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_static_call_matrix.py --check)
set_tests_properties(tools/gen_static_call_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_unary_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_unary_matrix.py --check)
set_tests_properties(tools/gen_unary_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_value_event_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_value_event_matrix.py --check)
set_tests_properties(tools/gen_value_event_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_value_argument_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_value_argument_matrix.py --check)
set_tests_properties(tools/gen_value_argument_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_default_init_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_default_init_matrix.py --check)
set_tests_properties(tools/gen_default_init_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_scope_name_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_scope_name_matrix.py --check)
set_tests_properties(tools/gen_scope_name_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_aggregate_compare_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_aggregate_compare_matrix.py --check)
set_tests_properties(tools/gen_aggregate_compare_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_aggregate_assign_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_aggregate_assign_matrix.py --check)
set_tests_properties(tools/gen_aggregate_assign_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_gate_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_gate_matrix.py --check)
set_tests_properties(tools/gen_gate_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_override_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_override_matrix.py --check)
set_tests_properties(tools/gen_override_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_fork_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_fork_matrix.py --check)
set_tests_properties(tools/gen_fork_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_argument_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_argument_matrix.py --check)
set_tests_properties(tools/gen_argument_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_udp_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_udp_matrix.py --check)
set_tests_properties(tools/gen_udp_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_udp_array_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_udp_array_matrix.py --check)
set_tests_properties(tools/gen_udp_array_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_udp_delay_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_udp_delay_matrix.py --check)
set_tests_properties(tools/gen_udp_delay_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_file_output_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_file_output_matrix.py --check)
set_tests_properties(tools/gen_file_output_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_file_close_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_file_close_matrix.py --check)
set_tests_properties(tools/gen_file_close_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_file_not_open_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_file_not_open_matrix.py --check)
set_tests_properties(tools/gen_file_not_open_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/mutants_apply
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/mutants.py --check)
set_tests_properties(tools/mutants_apply PROPERTIES LABELS Tools)

add_test(NAME tools/gen_random_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_random_matrix.py --check)
set_tests_properties(tools/gen_random_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_dump_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_dump_matrix.py --check)
set_tests_properties(tools/gen_dump_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_fread_struct_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_fread_struct_matrix.py --check)
set_tests_properties(tools/gen_fread_struct_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_randcase_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_randcase_matrix.py --check)
set_tests_properties(tools/gen_randcase_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_urandom_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_urandom_matrix.py --check)
set_tests_properties(tools/gen_urandom_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_memory_enum_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_memory_enum_matrix.py --check)
set_tests_properties(tools/gen_memory_enum_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_memory_file_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_memory_file_matrix.py --check)
set_tests_properties(tools/gen_memory_file_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_time_query_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_time_query_matrix.py --check)
set_tests_properties(tools/gen_time_query_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_file_input_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_file_input_matrix.py --check)
set_tests_properties(tools/gen_file_input_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_delay_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_delay_matrix.py --check)
set_tests_properties(tools/gen_delay_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_alias_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_alias_matrix.py --check)
set_tests_properties(tools/gen_alias_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_driver_start_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_driver_start_matrix.py --check)
set_tests_properties(tools/gen_driver_start_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_wait_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_wait_matrix.py --check)
set_tests_properties(tools/gen_wait_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_delay_value_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_delay_value_matrix.py --check)
set_tests_properties(tools/gen_delay_value_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_param_array_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_param_array_matrix.py --check)
set_tests_properties(tools/gen_param_array_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_postponed_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_postponed_matrix.py --check)
set_tests_properties(tools/gen_postponed_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_disable_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_disable_matrix.py --check)
set_tests_properties(tools/gen_disable_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_deferred_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_deferred_matrix.py --check)
set_tests_properties(tools/gen_deferred_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_typename_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_typename_matrix.py --check)
set_tests_properties(tools/gen_typename_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_aggregate_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_aggregate_call_matrix.py --check)
set_tests_properties(tools/gen_aggregate_call_matrix PROPERTIES LABELS Tools)

# The benchmark designs still run, and tools/bench.py still measures them
# (PERFORMANCE.md 5). A small count keeps it quick.
add_test(NAME tools/bench_smoke
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/bench.py
        --kirimasim $<TARGET_FILE:kirimasim> --n 100 --runs 1
        --out ${CMAKE_CURRENT_BINARY_DIR}/bench_smoke)
set_tests_properties(tools/bench_smoke PROPERTIES LABELS Tools)

add_test(NAME tools/gen_program_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_program_matrix.py --check)
set_tests_properties(tools/gen_program_matrix PROPERTIES LABELS Tools)
add_test(NAME tools/gen_unsupported_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_unsupported_matrix.py --check)
set_tests_properties(tools/gen_unsupported_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_process_lifetime_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_process_lifetime_matrix.py --check)
set_tests_properties(tools/gen_process_lifetime_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_wide_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_wide_matrix.py --check)
set_tests_properties(tools/gen_wide_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_illegal_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_illegal_matrix.py --check)
set_tests_properties(tools/gen_illegal_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_logic_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_logic_matrix.py --check)
set_tests_properties(tools/gen_logic_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_loop_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_loop_matrix.py --check)
set_tests_properties(tools/gen_loop_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_real_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_real_matrix.py --check)
set_tests_properties(tools/gen_real_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_string_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_string_matrix.py --check)
set_tests_properties(tools/gen_string_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_string_format_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_string_format_matrix.py --check)
set_tests_properties(tools/gen_string_format_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_real_convert_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_real_convert_matrix.py --check)
set_tests_properties(tools/gen_real_convert_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_rtoi_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_rtoi_matrix.py --check)
set_tests_properties(tools/gen_rtoi_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_format_source_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_format_source_matrix.py --check)
set_tests_properties(tools/gen_format_source_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_empty_argument_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_empty_argument_matrix.py --check)
set_tests_properties(tools/gen_empty_argument_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_time_format_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_time_format_matrix.py --check)
set_tests_properties(tools/gen_time_format_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_hier_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_hier_call_matrix.py --check)
set_tests_properties(tools/gen_hier_call_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_array_argument_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_array_argument_matrix.py --check)
set_tests_properties(tools/gen_array_argument_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_flush_point_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_flush_point_matrix.py --check)
set_tests_properties(tools/gen_flush_point_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_runtime_error_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_runtime_error_matrix.py --check)
set_tests_properties(tools/gen_runtime_error_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_format_flag_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_format_flag_matrix.py --check)
set_tests_properties(tools/gen_format_flag_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_subroutine_variable_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_subroutine_variable_matrix.py --check)
set_tests_properties(tools/gen_subroutine_variable_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_watch_reads_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_watch_reads_matrix.py --check)
set_tests_properties(tools/gen_watch_reads_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_builtin_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_builtin_call_matrix.py --check)
set_tests_properties(tools/gen_builtin_call_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_cond_op_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_cond_op_matrix.py --check)
set_tests_properties(tools/gen_cond_op_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_ref_wait_layout_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_ref_wait_layout_matrix.py --check)
set_tests_properties(tools/gen_ref_wait_layout_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_packed_range_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_packed_range_matrix.py --check)
set_tests_properties(tools/gen_packed_range_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_scan_wide_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_scan_wide_matrix.py --check)
set_tests_properties(tools/gen_scan_wide_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_string_char_op_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_string_char_op_matrix.py --check)
set_tests_properties(tools/gen_string_char_op_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_signed_cast_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_signed_cast_matrix.py --check)
set_tests_properties(tools/gen_signed_cast_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_math_wide_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_math_wide_matrix.py --check)
set_tests_properties(tools/gen_math_wide_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_case_compare_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_case_compare_matrix.py --check)
set_tests_properties(tools/gen_case_compare_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_static_init_report_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_static_init_report_matrix.py --check)
set_tests_properties(tools/gen_static_init_report_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_delay_overflow_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_delay_overflow_matrix.py --check)
set_tests_properties(tools/gen_delay_overflow_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_zero_time_loop_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_zero_time_loop_matrix.py --check)
set_tests_properties(tools/gen_zero_time_loop_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_interrupt_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_interrupt_matrix.py --check)
set_tests_properties(tools/gen_interrupt_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_call_depth_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_call_depth_matrix.py --check)
set_tests_properties(tools/gen_call_depth_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_cont_self_read_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_cont_self_read_matrix.py --check)
set_tests_properties(tools/gen_cont_self_read_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_config_top_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_config_top_matrix.py --check)
set_tests_properties(tools/gen_config_top_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_unconnected_drive_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_unconnected_drive_matrix.py --check)
set_tests_properties(tools/gen_unconnected_drive_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_negative_real_delay_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_negative_real_delay_matrix.py --check)
set_tests_properties(tools/gen_negative_real_delay_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_static_init_order_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_static_init_order_matrix.py --check)
set_tests_properties(tools/gen_static_init_order_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_later_work_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_later_work_matrix.py --check)
set_tests_properties(tools/gen_later_work_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_later_work_override_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_later_work_override_matrix.py --check)
set_tests_properties(tools/gen_later_work_override_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_real_delay_rounding_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_real_delay_rounding_matrix.py --check)
set_tests_properties(tools/gen_real_delay_rounding_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_cont_function_write_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_cont_function_write_matrix.py --check)
set_tests_properties(tools/gen_cont_function_write_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_block_position_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_block_position_matrix.py --check)
set_tests_properties(tools/gen_block_position_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_comb_sensitivity_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_comb_sensitivity_matrix.py --check)
set_tests_properties(tools/gen_comb_sensitivity_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_task_write_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_task_write_matrix.py --check)
set_tests_properties(tools/gen_task_write_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_assertion_wait_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_assertion_wait_matrix.py --check)
set_tests_properties(tools/gen_assertion_wait_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_deferred_write_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_deferred_write_matrix.py --check)
set_tests_properties(tools/gen_deferred_write_matrix PROPERTIES LABELS Tools)
