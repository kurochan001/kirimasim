// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Independently written declarations for the approved P1 VPI subset.
// ABI facts are checked by separately compiled, original SDK probes.
#ifndef KIRIMASIM_VPI_USER_H
#define KIRIMASIM_VPI_USER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t PLI_INT32;
typedef uint32_t PLI_UINT32;
typedef int64_t PLI_INT64;
typedef uint64_t PLI_UINT64;
typedef char PLI_BYTE8;
typedef double PLI_REAL64;
typedef struct kirima_vpi_opaque* vpiHandle;
typedef struct t_vpi_vecval {
    PLI_INT32 aval, bval;
} s_vpi_vecval, *p_vpi_vecval;
typedef struct t_vpi_time {
    PLI_INT32 type;
    PLI_UINT32 high, low;
    double real;
} s_vpi_time, *p_vpi_time;
typedef struct t_vpi_value {
    PLI_INT32 format;
    union {
        PLI_BYTE8* str;
        PLI_INT32 scalar, integer;
        double real;
        p_vpi_time time;
        p_vpi_vecval vector;
        void* misc;
    } value;
} s_vpi_value, *p_vpi_value;
typedef struct t_vpi_systf_data {
    PLI_INT32 type, sysfunctype;
    PLI_BYTE8* tfname;
    PLI_INT32 (*calltf)(PLI_BYTE8*);
    PLI_INT32 (*compiletf)(PLI_BYTE8*);
    PLI_INT32 (*sizetf)(PLI_BYTE8*);
    PLI_BYTE8* user_data;
} s_vpi_systf_data, *p_vpi_systf_data;
typedef struct t_cb_data {
    PLI_INT32 reason;
    PLI_INT32 (*cb_rtn)(struct t_cb_data*);
    vpiHandle obj;
    p_vpi_time time;
    p_vpi_value value;
    PLI_INT32 index;
    PLI_BYTE8* user_data;
} s_cb_data, *p_cb_data;
typedef struct t_vpi_error_info {
    PLI_INT32 state, level;
    PLI_BYTE8 *message, *product, *code, *file;
    PLI_INT32 line;
} s_vpi_error_info, *p_vpi_error_info;
typedef struct t_vpi_vlog_info {
    PLI_INT32 argc;
    PLI_BYTE8** argv;
    PLI_BYTE8 *product, *version;
} s_vpi_vlog_info, *p_vpi_vlog_info;
#define vpiSysTask 1
#define vpiSysFunc 2
#define vpiIntFunc 1
#define vpiRealFunc 2
#define vpiTimeFunc 3
#define vpiSizedFunc 4
#define vpiSizedSignedFunc 5
#define vpiType 1
#define vpiName 2
#define vpiFullName 3
#define vpiSize 4
#define vpiSigned 65
#define vpiFile 5
#define vpiLineNo 6
#define vpiTimeUnit 11
#define vpiTimePrecision 12
#define vpiConstType 40
#define vpiScalar 17
#define vpiVector 18
#define vpiModule 32
#define vpiReg 48
#define vpiNet 36
#define vpiIntegerVar 25
#define vpiRealVar 47
#define vpiMemory 29
#define vpiMemoryWord 30
#define vpiConstant 7
#define vpiSysTfCall 85
#define vpiSysFuncCall 56
#define vpiSysTaskCall 57
#define vpiScope 84
#define vpiArgument 89
#define vpiParent 81
#define vpiLeftRange 79
#define vpiRightRange 83
#define vpiIndex 78
#define vpiNamedBegin 33
#define vpiNamedFork 35
#define vpiTask 59
#define vpiFunction 20
#define vpiGenScope 134
#define vpiIterator 27
#define vpiNetBit 37
#define vpiRegBit 49
#define vpiBinStrVal 1
#define vpiOctStrVal 2
#define vpiDecStrVal 3
#define vpiHexStrVal 4
#define vpiScalarVal 5
#define vpiIntVal 6
#define vpiRealVal 7
#define vpiStringVal 8
#define vpiVectorVal 9
#define vpiSuppressVal 13
#define vpiObjTypeVal 12
#define vpi0 0
#define vpi1 1
#define vpiX 3
#define vpiZ 2
#define vpiSimTime 2
#define vpiScaledRealTime 1
#define vpiSuppressTime 3
#define vpiNoDelay 1
#define vpiInertialDelay 2
#define vpiTransportDelay 3
#define vpiPureTransportDelay 4
#define vpiForceFlag 5
#define vpiReleaseFlag 6
#define vpiReturnEvent 4096
#define cbValueChange 1
#define cbReadWriteSynch 6
#define cbReadOnlySynch 7
#define cbNextSimTime 8
#define cbAfterDelay 9
#define cbStartOfSimulation 11
#define cbEndOfSimulation 12
#define cbEndOfCompile 10
#define vpiStop 66
#define vpiFinish 67
#define vpiCompile 1
#define vpiRun 3
#define vpiNotice 1
#define vpiWarning 2
#define vpiError 3
#define vpiSystem 4
#define vpiInternal 5
vpiHandle vpi_register_systf(p_vpi_systf_data);
vpiHandle vpi_register_cb(p_cb_data);
PLI_INT32 vpi_remove_cb(vpiHandle);
vpiHandle vpi_handle(PLI_INT32, vpiHandle);
vpiHandle vpi_handle_by_name(PLI_BYTE8*, vpiHandle);
vpiHandle vpi_handle_by_index(vpiHandle, PLI_INT32);
vpiHandle vpi_iterate(PLI_INT32, vpiHandle);
vpiHandle vpi_scan(vpiHandle);
PLI_INT32 vpi_free_object(vpiHandle);
PLI_INT32 vpi_get(PLI_INT32, vpiHandle);
PLI_BYTE8* vpi_get_str(PLI_INT32, vpiHandle);
void vpi_get_value(vpiHandle, p_vpi_value);
vpiHandle vpi_put_value(vpiHandle, p_vpi_value, p_vpi_time, PLI_INT32);
void vpi_get_time(vpiHandle, p_vpi_time);
PLI_INT32 vpi_get_vlog_info(p_vpi_vlog_info);
PLI_INT32 vpi_chk_error(p_vpi_error_info);
PLI_INT32 vpi_printf(const PLI_BYTE8*, ...);
PLI_INT32 vpi_control(PLI_INT32, ...);
#ifdef __cplusplus
}
#endif
#endif
