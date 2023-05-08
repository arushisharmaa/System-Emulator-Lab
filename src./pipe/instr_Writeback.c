/**************************************************************************
 * C S 429 architecture emulator
 * 
 * instr_Writeback.c - Writeback stage of instruction processing pipeline.cs
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

extern int64_t W_wval;

/*
 * Write-back stage logic.
 * STUDENT TO-DO:
 * Implement the writeback stage.
 * 
 * Use in as the input pipeline register.
 * 
 * You will need the global variable W_wval.
 */

// This function updates the value in the write-back stage of the pipeline.
comb_logic_t wback_instr(w_instr_impl_t *in)
{
    // It checks if the value should come from memory or the execution stage, based on the control signals in the input.
    if(in->W_sigs.wval_sel){
        W_wval = in->val_mem;
    // If wval_sel is true, the value from memory (val_mem) is selected, otherwise the value from the execution stage (val_ex) is selected.
    }else{
        W_wval = in->val_ex;
    }
    return;
}