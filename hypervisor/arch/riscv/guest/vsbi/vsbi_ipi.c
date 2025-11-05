/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>
#include <vm.h>
#include <per_cpu.h>
#include <asm/sbi.h>
#include <asm/guest/vsbi.h>
#include <asm/guest/virq.h>
#include <logmsg.h>

static int32_t vcpu_sbi_ipi_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
	uint64_t func_id, uint64_t *args, __unused struct vsbi_ret *out)
{
	int32_t ret = SBI_SUCCESS;
	uint16_t i;
	struct acrn_vcpu *tmp_vcpu;
	uint64_t harts_mask = args[0];
	uint64_t harts_base = args[1];
	uint64_t hart_bit = 0, sentmask = 0;
	int early_exit = 0;
	int use_mask = (harts_base != UINT64_MAX);

	switch (func_id) {
	case SBI_IPI_FID_SEND_IPI:
		foreach_vcpu(i, vcpu->vm, tmp_vcpu) {
			if (use_mask) {
				if (tmp_vcpu->vcpu_id < harts_base)
					continue;
				hart_bit = tmp_vcpu->vcpu_id - harts_base;
				if (hart_bit >= 64UL) {
					early_exit = 1;
					break;
				}
				if (!(harts_mask & (1UL << hart_bit)))
					continue;
			}

			/* VSSIP:2 asserts a VS-level software interrupt to target VCPU */
			ret = vcpu_set_intr(tmp_vcpu, 2);
			if (ret < 0) {
				pr_err("vsbi ipi: failed to send ipi to vcpu %hu", tmp_vcpu->vcpu_id);
				break;
			}

			if (use_mask)
				sentmask |= 1UL << hart_bit;
		}

		if (use_mask && !early_exit && (harts_mask ^ sentmask))
			ret = SBI_ERR_INVALID_PARAM;
		else if (use_mask && early_exit)
			ret = SBI_ERR_INVALID_PARAM;
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_ipi = {
	.name = "ipi",
	.eid_start = SBI_EID_IPI,
	.eid_end = SBI_EID_IPI,
	.handler = vcpu_sbi_ipi_ecall_handler,
};
