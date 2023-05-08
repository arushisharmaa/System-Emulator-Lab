/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
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
extern uint64_t F_PC;

/*
 * Select PC logic.
 * STUDENT TO-DO:
 * Write the next PC to *current_PC.
 */
static comb_logic_t select_PC(uint64_t pred_PC,                                      // The predicted PC
                              opcode_t D_opcode, uint64_t val_a,                     // Possible correction from RET
                              opcode_t M_opcode, bool M_cond_val, uint64_t seq_succ, // Possible correction from B.cond
                              uint64_t *current_PC)
{
    /*
     * Students: Please leave this code
     * at the top of this function.
     * You may modify below it.
     */
    if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR)
    {
        *current_PC = 0; // PC can't be 0 normally.
        return;
    }
    // // Modify starting here.
    if (D_opcode == OP_RET)
    { // getting right RA
        *current_PC = val_a;
        return;
    }
    // This function modifies the current program counter (PC) based on the instruction executed in the previous cycle.
    // Modify starting here.
    // If the instruction was a conditional branch and the condition was not met, then the PC is set to the sequential successor.
    if ((!M_cond_val) && (M_opcode == OP_B_COND))
    { // mispredicted branch
        *current_PC = seq_succ;
        return;
    }
    *current_PC = pred_PC; // correct prediction
    return;
}

/*
 * Predict PC logic. Conditional branches are predicted taken.
 * STUDENT TO-DO:
 * Write the predicted next PC to *predicted_PC
 * and the next sequential pc to *seq_succ.
 */
static comb_logic_t predict_PC(uint64_t current_PC, uint32_t insnbits, opcode_t op,
                               uint64_t *predicted_PC, uint64_t *seq_succ)
{
    /*
     * Students: Please leave this code
     * at the top of this function.
     * You may modify below it.
     */
    if (!current_PC)
    {
        return; // We use this to generate a halt instruction.
    }
    // Modify starting here.
    if ((op == OP_BL) || (op == OP_B) || (op == OP_B_COND))
    {
        int temp_val;
        if ((op == OP_BL) || (op == OP_B))
        { // unconditional branch (b or bl)
            temp_val = (0x03FFFFFF & insnbits) << 2;
            if ((temp_val >> 27) != 0)
            {
                temp_val = 0xF8000000 | temp_val;
            }
        }
        else if (op == OP_B_COND)
        { // conditional branch
            temp_val = (0xFFFFE0 & insnbits) >> 3;
            if ((0x100000 & temp_val) != 0)
            {
                temp_val = 0xFFE00000 | temp_val;
            }
        }
        // set the seq_succ & the predicted_pc value currently
        *seq_succ = current_PC + 4;
        *predicted_PC = current_PC + temp_val;
    }
    else
    {
        // do a double check with those values
        *seq_succ = current_PC + 4;
        *predicted_PC = current_PC + 4;
    }
    return;
}

/*
 * Helper function to recognize the aliased instructions:
 * LSL, LSR, CMP, and TST. We do this only to simplify the
 * implementations of the shift operations (rather than having
 * to implement UBFM in full).
 */
static void fix_instr_aliases(uint32_t insnbits, opcode_t *op)
{
    // check the value of UBFM to determine if it's a left shift or a right shift
    if (*op == OP_UBFM)
    {
        if ((0x1FU & (insnbits >> 10)) == 31)
        {
            *op = OP_LSR;
        }
        else
        {
            *op = OP_LSL;
        }
    }
    // set the RR or the shifts (OR)
    else if ((*op == OP_SUBS_RR) && ((insnbits & 0x1FU) == 31))
    {
        *op = OP_CMP_RR;
    }
    // set the second RR or the shift (AND)
    else if ((*op == OP_ANDS_RR) && (((insnbits & 0x1FU) == 31)))
    {
        *op = OP_TST_RR;
    }
    return;
}

/*
 * Fetch stage logic.
 * STUDENT TO-DO:
 * Implement the fetch stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * Additionally, update F_PC for the next
 * cycle's predicted PC.
 *
 * You will also need the following helper functions:
 * select_pc, predict_pc, and imem.
 */

comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out)
{
    // set the values
    bool imem_error = 0;
    uint64_t current_PC;
    select_PC(in->pred_PC, X_out->op, M_in->val_ex, M_out->op, M_out->cond_holds, M_out->seq_succ_PC, &current_PC);

    /*
     * Students: This case is for generating HLT instructions
     * to stop the pipeline. Only write your code in the **else** case.
     */
    if (!current_PC)
    {
        // set the values correctly -> HLT
        out->insnbits = 0xD4400000U;
        out->op = OP_HLT;
        out->print_op = OP_HLT;
        imem_error = false;
    }
    else
    {
        uint32_t instr;
        imem(current_PC, &instr, &imem_error);
        // set the correct index with the right shift
        int op_index = 0x7FF & (instr >> 21);
        opcode_t opcode_val = itable[op_index];
        fix_instr_aliases(instr, &opcode_val);
        // check to make sure the opcode is a real value
        if ((opcode_val == -1) || imem_error)
        {
            // set the specific values since it's not an error
            out->print_op = opcode_val;
            out->op = opcode_val;
            out->seq_succ_PC = current_PC + 4;
            F_PC = out->seq_succ_PC;
            out->status = STAT_INS;
            in->status = STAT_INS;
        }
        else
        {
            predict_PC(current_PC, instr, opcode_val, &F_PC, &(out->seq_succ_PC));
            out->op = opcode_val;
            out->print_op = opcode_val;
            out->insnbits = instr;
            out->status = STAT_AOK;
        }
    }
    // HLT STATUS PASS->specific conditions for HLT
    if (out->op == OP_HLT)
    {
        in->status = STAT_HLT;
        out->status = STAT_HLT;
        out->op = OP_HLT;
    }

    return;
}
