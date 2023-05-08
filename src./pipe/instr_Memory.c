/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Memory.c - Memory stage of instruction processing pipeline.cs
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"

extern machine_t guest;
extern mem_status_t dmem_status;

extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

/*
 * Memory stage logic.
 * STUDENT TO-DO:
 * Implement the memory stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 *
 * You will need the following helper functions:
 * copy_w_ctl_signals and dmem.e
 */

// This function executes the memory stage of the pipeline for memory instructions
//  It takes the input memory instruction and produces the output writeback instruction
comb_logic_t memory_instr(m_instr_impl_t *in, w_instr_impl_t *out)
{
    // Copy the control signals for the writeback stage from the input to the output
    copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));
    // Copy the destination register and instruction operation from the input to the output
    out->dst = in->dst;
    out->op = in->op;

    // Copy the print_op flag from the input to the output
    out->print_op = in->print_op;

    bool dmem_err;

    // If the memory instruction requires a data memory read or write, call the dmem function to execute it
    if (in->M_sigs.dmem_write || in->M_sigs.dmem_read)
    {
        dmem(in->val_ex, in->val_b, (in->M_sigs).dmem_read, (in->M_sigs).dmem_write, &(out->val_mem), &dmem_err);
    }

    // Copy the value of register B and the result of the ALU operation from the input to the output
    out->val_b = in->val_b;
    out->val_ex = in->val_ex;

    // If there was a data memory error, set the status to STAT_ADR and return
    if (dmem_err)
    {
        if (in->M_sigs.dmem_write || in->M_sigs.dmem_read)
        {
            out->status = STAT_ADR;
            return;
        }
    }

    // Copy the status from the input to the output
    out->status = in->status;

    return;
}