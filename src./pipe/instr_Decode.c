/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Decode.c - Decode stage of instruction processing pipeline.
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
#include "forward.h"
#include "machine.h"
#include "hw_elts.h"

#define SP_NUM 31
#define XZR_NUM 32

extern machine_t guest;
extern mem_status_t dmem_status;

extern int64_t W_wval;

/*
 * Control signals for D, X, M, and W stages.
 * Generated by D stage logic.
 * D control signals are consumed locally.
 * Others must be buffered in pipeline registers.
 * STUDENT TO-DO:
 * Generate the correct control signals for this instruction's
 * future stages and write them to the corresponding struct.
 */

// This function generates control signals for the DXMW pipeline stages of a processor
// based on the opcode of an instruction.
static comb_logic_t generate_DXMW_control(opcode_t op, x_ctl_sigs_t *X_sigs, m_ctl_sigs_t *M_sigs, w_ctl_sigs_t *W_sigs)
{
    if (op == OP_BL)
    {
        W_sigs->dst_sel = 1;
    }
    else
    {
        W_sigs->dst_sel = 0;
    }
    if (op == OP_LDUR)
    {
        W_sigs->wval_sel = 1;
    }
    else
    {
        W_sigs->wval_sel = 0;
    }
    switch (op)
    {
    case OP_B:
    case OP_B_COND:
    case OP_STUR:
    case OP_NOP:
    case OP_RET:
        W_sigs->w_enable = 0;
        break;
    default:
        W_sigs->w_enable = 1;
        break;
    }
    switch (op)
    {
    case OP_CMP_RR:
    case OP_TST_RR:
    case OP_SUBS_RR:
    case OP_ORR_RR:
    case OP_EOR_RR:
    case OP_ADDS_RR:
    case OP_ANDS_RR:
    case OP_MVN:
        X_sigs->valb_sel = 1;
        break;
    default:
        X_sigs->valb_sel = 0;
        break;
    }
    switch (op)
    {
    case OP_ANDS_RR:
    case OP_SUBS_RR:
    case OP_CMP_RR:
    case OP_ADDS_RR:
    case OP_TST_RR:
        X_sigs->set_CC = 1;
        break;
    default:
        X_sigs->set_CC = 0;
        break;
    }
    M_sigs->dmem_write = 0;
    M_sigs->dmem_read = 0;
    if (op == OP_STUR)
    {
        M_sigs->dmem_write = 1;
    }
    else if (op == OP_LDUR)
    {
        M_sigs->dmem_read = 1;
    }
    return;
}

/*
 * Logic for extracting the immediate value for M-, I-, and RI-format instructions.
 * STUDENT TO-DO:
 * Extract the immediate value and write it to *imm.
 */
// This function extracts the immediate value from the instruction bits based on the opcode type
static comb_logic_t extract_immval(uint32_t insnbits, opcode_t op, int64_t *imm)
{
    switch (op)
    {
    case OP_MOVZ:
    case OP_MOVK:
        *imm = 0xFFFFU & (insnbits >> 5);
        break;
    case OP_LDUR:
    case OP_STUR:
        *imm = 0x1FFU & (insnbits >> 12);
        break;
    case OP_LSL:
        *imm = 64 - (0x3F & (insnbits >> 16));
        break;
    case OP_B_COND:
        *imm = (0x7FFFF & (insnbits >> 5));
        break;
    case OP_ASR:
    case OP_LSR:
        *imm = (0x3F & (insnbits >> 16));
        break;
    case OP_B:
    case OP_BL:
        *imm = (0x3FFFFFF & insnbits);
        break;
    case OP_ADRP:
        printf("filler");
        unsigned int low = (0x3 & (insnbits >> 29)) << 12;
        unsigned int high = (0x7FFFF & (insnbits >> 5)) << 14;
        *imm = high | low;
        break;
    case OP_ADD_RI:
    case OP_SUB_RI:
        *imm = (0xFFF & (insnbits >> 10));
        break;
    default:
        *imm = 0;
        break;
    }
    return;
}

/*
 * Logic for determining the ALU operation needed for this opcode.
 * STUDENT TO-DO:
 * Determine the ALU operation based on the given opcode
 * and write it to *ALU_op.
 */
static comb_logic_t decide_alu_op(opcode_t op, alu_op_t *ALU_op)
{
    switch (op)
    {
    case OP_LSR:
        *ALU_op = LSR_OP;
        break;
    case OP_CMP_RR:
    case OP_SUB_RI:
    case OP_SUBS_RR:
        *ALU_op = MINUS_OP;
        break;
    case OP_MVN:
        *ALU_op = NEG_OP;
        break;
    case OP_EOR_RR:
        *ALU_op = EOR_OP;
        break;
    case OP_ADD_RI:
    case OP_LDUR:
    case OP_STUR:
    case OP_ADRP:
    case OP_ADDS_RR:
        *ALU_op = PLUS_OP;
        break;
    case OP_TST_RR:
    case OP_ANDS_RR:
        *ALU_op = AND_OP;
        break;
    case OP_ASR:
        *ALU_op = ASR_OP;
        break;
    case OP_MOVZ:
    case OP_MOVK:
        *ALU_op = MOV_OP;
        break;
    case OP_LSL:
        *ALU_op = LSL_OP;
        break;
    case OP_ORR_RR:
        *ALU_op = OR_OP;
        break;
    default:
        *ALU_op = PASS_A_OP;
        break;
    }
    return;
}

/*
 * Utility functions for copying over control signals across a stage.
 * STUDENT TO-DO:
 * Copy the input signals from the input side of the pipeline
 * register to the output side of the register.
 */
comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *dest, m_ctl_sigs_t *src)
{
    dest->dmem_write = src->dmem_write;
    dest->dmem_read = src->dmem_read;
    return;
}

comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *dest, w_ctl_sigs_t *src)
{
    dest->wval_sel = src->wval_sel;
    dest->dst_sel = src->dst_sel;
    dest->w_enable = src->w_enable;
    return;
}
// The function takes in four arguments:
// - insnbits: the instruction bits to extract the register numbers from
// - op: the opcode that the instruction corresponds to
// - src1: a pointer to an unsigned 8-bit integer to store the first source register number
// - src2: a pointer to an unsigned 8-bit integer to store the second source register number
// - dst: a pointer to an unsigned 8-bit integer to store the destination register number
comb_logic_t extract_regs(uint32_t insnbits, opcode_t op, uint8_t *src1, uint8_t *src2, uint8_t *dst)
{
    *src1 = 32;
    *src2 = 32;
    *dst = insnbits & 0x1F;
    switch (op)
    {
    case OP_TST_RR:
    case OP_B_COND:
    case OP_NOP:
    case OP_RET:
    case OP_B:
    case OP_STUR:
    case OP_CMP_RR:
        *dst = 32;
        break;
    default:
        break;
    }
    switch (op)
    {
    case OP_ASR:
    case OP_LSR:
    case OP_UBFM:
    case OP_SUB_RI:
    case OP_ADD_RI:
    case OP_LSL:
    case OP_LDUR:
        *src1 = 0x1F & (insnbits >> 5);
        break;
    case OP_MOVZ:
        *src1 = 32;
        break;
    case OP_MOVK:
        *src1 = *dst;
        break;
    case OP_STUR:
        *src2 = 0x1F & insnbits;
        *src1 = 0x1F & (insnbits >> 5);
        break;
    case OP_MVN:
        *src1 = 32;
        *src2 = 0x1f & (insnbits >> 16);
        if (*src1 == 31)
        {
            *src1 = 32;
        }
        if (*src2 == 31)
        {
            *src2 = 32;
        }
        if (*dst == 31)
        {
            *dst = 32;
        }
        break;
        // RR value for destination
    case OP_SUBS_RR:
    case OP_TST_RR:
    case OP_ANDS_RR:
    case OP_CMP_RR:
    case OP_ORR_RR:
    case OP_ADDS_RR:
    case OP_EOR_RR:
        *src2 = 0x1F & (insnbits >> 16);
        *src1 = 0x1F & (insnbits >> 5);
        if (*dst == 31)
        {
            *dst = 32;
        }
        if (*src1 == 31)
        {
            *src1 = 32;
        }
        if (*src2 == 31)
        {
            *src2 = 32;
        }
        break;
        // reset the destistination value
    case OP_ADRP:
        *dst = 0x1F & insnbits;
        break;
    case OP_BL:
        *dst = 30;
        break;
    case OP_RET:
        *src1 = 0x1F & (insnbits >> 5);
        break;
    default:
        break;
    }
    return;
}

/*
 * Decode stage logic.
 * STUDENT TO-DO:
 * Implement the decode stage.
 *
 * Use `in` as the input pipeline register,
 * and update the `out` pipeline register as output.
 * Additionally, make sure the register file is updated
 * with W_out's output when you call it in this stage.
 *
 * You will also need the following helper functions:
 * generate_DXMW_control, regfile, extract_immval,
 * and decide_alu_op.
 */
comb_logic_t decode_instr(d_instr_impl_t *in, x_instr_impl_t *out)
{
    // update the status at the beginning
    out->status = in->status;
    // only implement if these are the two status conditions
    if ((in->status == STAT_BUB) || (in->status == STAT_AOK))
    {
        out->seq_succ_PC = in->seq_succ_PC;
        out->op = in->op;
        out->print_op = in->print_op;
        out->cond = 0xF & in->insnbits;
        // update the values using the helper methods
        extract_immval(in->insnbits, in->op, &(out->val_imm));
        decide_alu_op(in->op, &(out->ALU_op));
        generate_DXMW_control(in->op, &(out->X_sigs), &(out->M_sigs), &(out->W_sigs));
        uint8_t src_reg1;
        uint8_t src_reg2;
        extract_regs(in->insnbits, in->op, &src_reg1, &src_reg2, &(out->dst));
        if ((W_out->status == STAT_BUB) || (W_out->status == STAT_AOK))
        {
            regfile(src_reg1, src_reg2, W_out->dst, W_wval, W_out->W_sigs.w_enable, &(out->val_a), &(out->val_b));
        }
        // use the new forward condtion
        forward_reg(src_reg1, src_reg2, X_out->dst, M_out->dst, W_out->dst, M_in->val_ex, M_out->val_ex, W_in->val_mem, W_out->val_ex, W_out->val_mem, M_out->W_sigs.wval_sel, W_out->W_sigs.wval_sel, X_out->W_sigs.w_enable, M_out->W_sigs.w_enable, W_out->W_sigs.w_enable, &(out->val_a), &(out->val_b));
        if ((in->op == OP_MOVZ) || (in->op == OP_MOVK))
        {
            out->val_hw = (0x3 & (in->insnbits >> 21)) << 4;
        }
        else
        {
            out->val_hw = 0;
        }
        // change the move value
        if (in->op == OP_MOVK)
        {
            unsigned long mask_bits = 0xFFFF;
            mask_bits = mask_bits << out->val_hw;
            mask_bits = ~mask_bits;
            out->val_a = out->val_a & mask_bits;
        }
        else if (in->op == OP_ADRP)
        {
            out->val_a = (in->seq_succ_PC - 4) & 0xFFFFFFFFFFFFF000;
        }
        else if (in->op == OP_BL)
        {
            out->val_a = in->seq_succ_PC;
        }
    }
    else
    {
        // update the values of the STATUS OF AOK and STATUS OF BUB to work
        uint8_t src_reg1 = 32;
        uint8_t src_reg2 = 32;
        if (W_out->status == STAT_AOK || W_out->status == STAT_BUB)
        {
            regfile(src_reg1, src_reg2, W_out->dst, W_wval, W_out->W_sigs.w_enable, &(out->val_a), &(out->val_b));
        }
        // change the op & the print_op
        out->op = in->op;
        out->print_op = in->print_op;
    }
    return;
}
