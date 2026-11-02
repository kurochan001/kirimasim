/* Copyright (c) 2026 The kirimasim Authors */
/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include <vpi_user.h>
#include <stdlib.h>

/* Original 38.34 probe: output storage is empty before each release. */
static PLI_INT32 release_probe(PLI_BYTE8 *unused) {
    (void)unused;
    vpiHandle args = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, 0));
    vpiHandle target = vpi_scan(args);
    (void)vpi_scan(args);
    const int formats[] = {vpiScalarVal, vpiIntVal, vpiVectorVal, vpiBinStrVal,
                           vpiHexStrVal, vpiRealVal};
    const int width = vpi_get(vpiSize, target);
    int errors = 0;
    for (unsigned f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {
        s_vpi_value force = {0};
        force.format = vpiIntVal;
        force.value.integer = 1;
        vpi_put_value(target, &force, 0, vpiForceFlag);
        if (vpi_chk_error(0)) ++errors;
        s_vpi_value result = {0};
        result.format = formats[f];
        vpi_put_value(target, &result, 0, vpiReleaseFlag);
        if (vpi_chk_error(0)) ++errors;
        if (result.format != formats[f]) {
            ++errors;
            continue;
        }
        switch (result.format) {
            case vpiScalarVal: errors += result.value.scalar != vpi1; break;
            case vpiIntVal: errors += result.value.integer != 1; break;
            case vpiRealVal: errors += result.value.real != 1.0; break;
            case vpiVectorVal:
                if (!result.value.vector) { ++errors; break; }
                for (int w = 0; w < (width + 31) / 32; ++w) {
                    errors += result.value.vector[w].aval != (w == 0 ? 1 : 0);
                    errors += result.value.vector[w].bval != 0;
                }
                break;
            default:
                errors += !result.value.str ||
                          strtoul(result.value.str, 0, result.format == vpiBinStrVal ? 2 : 16) != 1;
        }
    }
    vpi_printf("release errors=%d\n", errors);
    return 0;
}
static void startup(void) {
    s_vpi_systf_data data = {0};
    data.type = vpiSysTask;
    data.tfname = "$release_probe";
    data.calltf = release_probe;
    vpi_register_systf(&data);
}
void (*vlog_startup_routines[])(void) = {startup, 0};
