// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Independently observed public SDK ABI facts, 2026-11-01; no SDK dependency.
#include <stddef.h>
#include <stdint.h>
#include <vpi_user.h>

_Static_assert(vpiSysTask == 1, "vpiSysTask");
_Static_assert(vpiSysFunc == 2, "vpiSysFunc");
_Static_assert(vpiIntFunc == 1, "vpiIntFunc");
_Static_assert(vpiRealFunc == 2, "vpiRealFunc");
_Static_assert(vpiTimeFunc == 3, "vpiTimeFunc");
_Static_assert(vpiSizedFunc == 4, "vpiSizedFunc");
_Static_assert(vpiSizedSignedFunc == 5, "vpiSizedSignedFunc");
_Static_assert(vpiType == 1, "vpiType");
_Static_assert(vpiName == 2, "vpiName");
_Static_assert(vpiFullName == 3, "vpiFullName");
_Static_assert(vpiSize == 4, "vpiSize");
_Static_assert(vpiSigned == 65, "vpiSigned");
_Static_assert(vpiFile == 5, "vpiFile");
_Static_assert(vpiLineNo == 6, "vpiLineNo");
_Static_assert(vpiTimeUnit == 11, "vpiTimeUnit");
_Static_assert(vpiTimePrecision == 12, "vpiTimePrecision");
_Static_assert(vpiConstType == 40, "vpiConstType");
_Static_assert(vpiScalar == 17, "vpiScalar");
_Static_assert(vpiVector == 18, "vpiVector");
_Static_assert(vpiModule == 32, "vpiModule");
_Static_assert(vpiReg == 48, "vpiReg");
_Static_assert(vpiNet == 36, "vpiNet");
_Static_assert(vpiIntegerVar == 25, "vpiIntegerVar");
_Static_assert(vpiRealVar == 47, "vpiRealVar");
_Static_assert(vpiMemory == 29, "vpiMemory");
_Static_assert(vpiMemoryWord == 30, "vpiMemoryWord");
_Static_assert(vpiConstant == 7, "vpiConstant");
_Static_assert(vpiSysTfCall == 85, "vpiSysTfCall");
_Static_assert(vpiSysFuncCall == 56, "vpiSysFuncCall");
_Static_assert(vpiSysTaskCall == 57, "vpiSysTaskCall");
_Static_assert(vpiScope == 84, "vpiScope");
_Static_assert(vpiArgument == 89, "vpiArgument");
_Static_assert(vpiParent == 81, "vpiParent");
_Static_assert(vpiLeftRange == 79, "vpiLeftRange");
_Static_assert(vpiRightRange == 83, "vpiRightRange");
_Static_assert(vpiIndex == 78, "vpiIndex");
_Static_assert(vpiIterator == 27, "vpiIterator");
_Static_assert(vpiNetBit == 37, "vpiNetBit");
_Static_assert(vpiRegBit == 49, "vpiRegBit");
_Static_assert(vpiBinStrVal == 1, "vpiBinStrVal");
_Static_assert(vpiOctStrVal == 2, "vpiOctStrVal");
_Static_assert(vpiDecStrVal == 3, "vpiDecStrVal");
_Static_assert(vpiHexStrVal == 4, "vpiHexStrVal");
_Static_assert(vpiScalarVal == 5, "vpiScalarVal");
_Static_assert(vpiIntVal == 6, "vpiIntVal");
_Static_assert(vpiRealVal == 7, "vpiRealVal");
_Static_assert(vpiStringVal == 8, "vpiStringVal");
_Static_assert(vpiVectorVal == 9, "vpiVectorVal");
_Static_assert(vpiSuppressVal == 13, "vpiSuppressVal");
_Static_assert(vpiObjTypeVal == 12, "vpiObjTypeVal");
_Static_assert(vpi0 == 0, "vpi0");
_Static_assert(vpi1 == 1, "vpi1");
_Static_assert(vpiX == 3, "vpiX");
_Static_assert(vpiZ == 2, "vpiZ");
_Static_assert(vpiSimTime == 2, "vpiSimTime");
_Static_assert(vpiScaledRealTime == 1, "vpiScaledRealTime");
_Static_assert(vpiSuppressTime == 3, "vpiSuppressTime");
_Static_assert(vpiNoDelay == 1, "vpiNoDelay");
_Static_assert(vpiInertialDelay == 2, "vpiInertialDelay");
_Static_assert(vpiTransportDelay == 3, "vpiTransportDelay");
_Static_assert(vpiPureTransportDelay == 4, "vpiPureTransportDelay");
_Static_assert(vpiForceFlag == 5, "vpiForceFlag");
_Static_assert(vpiReleaseFlag == 6, "vpiReleaseFlag");
_Static_assert(vpiReturnEvent == 4096, "vpiReturnEvent");
_Static_assert(cbValueChange == 1, "cbValueChange");
_Static_assert(cbReadWriteSynch == 6, "cbReadWriteSynch");
_Static_assert(cbReadOnlySynch == 7, "cbReadOnlySynch");
_Static_assert(cbNextSimTime == 8, "cbNextSimTime");
_Static_assert(cbAfterDelay == 9, "cbAfterDelay");
_Static_assert(cbStartOfSimulation == 11, "cbStartOfSimulation");
_Static_assert(cbEndOfSimulation == 12, "cbEndOfSimulation");
_Static_assert(cbEndOfCompile == 10, "cbEndOfCompile");
_Static_assert(vpiStop == 66, "vpiStop");
_Static_assert(vpiFinish == 67, "vpiFinish");
_Static_assert(vpiCompile == 1, "vpiCompile");
_Static_assert(vpiRun == 3, "vpiRun");
_Static_assert(vpiNotice == 1, "vpiNotice");
_Static_assert(vpiWarning == 2, "vpiWarning");
_Static_assert(vpiError == 3, "vpiError");
_Static_assert(vpiSystem == 4, "vpiSystem");
_Static_assert(vpiInternal == 5, "vpiInternal");
_Static_assert(sizeof(PLI_INT32) == 4 && (PLI_INT32)-1 < 0, "signed 32-bit PLI");
_Static_assert(sizeof(PLI_UINT32) == 4 && (PLI_UINT32)-1 > 0, "unsigned 32-bit PLI");
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(PLI_INT32) == 4, "sizeof_PLI_INT32");
_Static_assert(sizeof(PLI_INT64) == 8, "sizeof_PLI_INT64");
_Static_assert(sizeof(s_vpi_vecval) == 8, "sizeof_s_vpi_vecval");
_Static_assert(sizeof(s_vpi_time) == 24, "sizeof_s_vpi_time");
_Static_assert(sizeof(s_vpi_value) == 16, "sizeof_s_vpi_value");
_Static_assert(sizeof(s_vpi_systf_data) == 48, "sizeof_s_vpi_systf_data");
_Static_assert(sizeof(s_cb_data) == 56, "sizeof_s_cb_data");
_Static_assert(sizeof(s_vpi_error_info) == 48, "sizeof_s_vpi_error_info");
_Static_assert(sizeof(s_vpi_vlog_info) == 32, "sizeof_s_vpi_vlog_info");
_Static_assert(offsetof(s_vpi_systf_data, type) == 0, "offset_s_vpi_systf_data_type");
_Static_assert(offsetof(s_vpi_systf_data, sysfunctype) == 4, "offset_s_vpi_systf_data_sysfunctype");
_Static_assert(offsetof(s_vpi_systf_data, tfname) == 8, "offset_s_vpi_systf_data_tfname");
_Static_assert(offsetof(s_vpi_systf_data, calltf) == 16, "offset_s_vpi_systf_data_calltf");
_Static_assert(offsetof(s_vpi_systf_data, compiletf) == 24, "offset_s_vpi_systf_data_compiletf");
_Static_assert(offsetof(s_vpi_systf_data, sizetf) == 32, "offset_s_vpi_systf_data_sizetf");
_Static_assert(offsetof(s_vpi_systf_data, user_data) == 40, "offset_s_vpi_systf_data_user_data");
_Static_assert(offsetof(s_cb_data, reason) == 0, "offset_s_cb_data_reason");
_Static_assert(offsetof(s_cb_data, cb_rtn) == 8, "offset_s_cb_data_cb_rtn");
_Static_assert(offsetof(s_cb_data, obj) == 16, "offset_s_cb_data_obj");
_Static_assert(offsetof(s_cb_data, time) == 24, "offset_s_cb_data_time");
_Static_assert(offsetof(s_cb_data, value) == 32, "offset_s_cb_data_value");
_Static_assert(offsetof(s_cb_data, index) == 40, "offset_s_cb_data_index");
_Static_assert(offsetof(s_cb_data, user_data) == 48, "offset_s_cb_data_user_data");
_Static_assert(offsetof(s_vpi_time, type) == 0, "offset_s_vpi_time_type");
_Static_assert(offsetof(s_vpi_time, high) == 4, "offset_s_vpi_time_high");
_Static_assert(offsetof(s_vpi_time, low) == 8, "offset_s_vpi_time_low");
_Static_assert(offsetof(s_vpi_time, real) == 16, "offset_s_vpi_time_real");
_Static_assert(offsetof(s_vpi_value, format) == 0, "offset_s_vpi_value_format");
_Static_assert(offsetof(s_vpi_value, value) == 8, "offset_s_vpi_value_value");
_Static_assert(offsetof(s_vpi_error_info, state) == 0, "offset_s_vpi_error_info_state");
_Static_assert(offsetof(s_vpi_error_info, level) == 4, "offset_s_vpi_error_info_level");
_Static_assert(offsetof(s_vpi_error_info, message) == 8, "offset_s_vpi_error_info_message");
_Static_assert(offsetof(s_vpi_error_info, product) == 16, "offset_s_vpi_error_info_product");
_Static_assert(offsetof(s_vpi_error_info, code) == 24, "offset_s_vpi_error_info_code");
_Static_assert(offsetof(s_vpi_error_info, file) == 32, "offset_s_vpi_error_info_file");
_Static_assert(offsetof(s_vpi_error_info, line) == 40, "offset_s_vpi_error_info_line");
_Static_assert(offsetof(s_vpi_vlog_info, argc) == 0, "offset_s_vpi_vlog_info_argc");
_Static_assert(offsetof(s_vpi_vlog_info, argv) == 8, "offset_s_vpi_vlog_info_argv");
_Static_assert(offsetof(s_vpi_vlog_info, product) == 16, "offset_s_vpi_vlog_info_product");
_Static_assert(offsetof(s_vpi_vlog_info, version) == 24, "offset_s_vpi_vlog_info_version");
#endif
_Static_assert(_Generic(&vpi_register_systf, vpiHandle (*)(const s_vpi_systf_data*): 1, default: 0),
               "vpi_register_systf C signature");
_Static_assert(_Generic(&vpi_register_cb, vpiHandle (*)(p_cb_data): 1, default: 0),
               "vpi_register_cb C signature");
_Static_assert(_Generic(&vpi_get, PLI_INT32 (*)(PLI_INT32, vpiHandle): 1, default: 0),
               "vpi_get C signature");
_Static_assert(_Generic(&vpi_get_value, void (*)(vpiHandle, p_vpi_value): 1, default: 0),
               "vpi_get_value C signature");
_Static_assert(_Generic(&vpi_put_value,
                   vpiHandle (*)(vpiHandle, p_vpi_value, p_vpi_time, PLI_INT32): 1,
                   default: 0),
               "vpi_put_value C signature");
_Static_assert(_Generic(&vpi_control, PLI_INT32 (*)(PLI_INT32, ...): 1, default: 0),
               "vpi_control C signature");
