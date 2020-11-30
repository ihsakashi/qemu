/*
 * QEMU HVF support -- ARM specific functions.
 *
 * Copyright (c) 2020 Tian Zhang
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_HVF_ARM_H
#define QEMU_HVF_ARM_H

#include "sysemu/hvf.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

#ifdef CONFIG_HVF

/**
 * hvf_arm_set_cpu_features_from_host:
 * @cpu: ARMCPU to set the features for
 *
 * Set up the ARMCPU struct fields up to match the information probed
 * from the host CPU.
 */
void hvf_arm_set_cpu_features_from_host(ARMCPU *cpu);

#else

/*
 * These functions should never actually be called without HVF support.
 */
static inline void hvf_arm_set_cpu_features_from_host(ARMCPU *cpu)
{
    g_assert_not_reached();
}

#endif

#endif
