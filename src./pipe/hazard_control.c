/**************************************************************************
 * C S 429 system emulator
 * 
 * Bubble and stall checking logic.
 * STUDENT TO-DO:
 * Implement logic for hazard handling.
 * 
 * handle_hazards is called from proc.c with the appropriate
 * parameters already set, you must implement the logic for it.
 * 
 * You may optionally use the other three helper functions to 
 * make it easier to follow your logic.
 **************************************************************************/ 

#include "machine.h"

extern machine_t guest;
extern mem_status_t dmem_status;

/* Use this method to actually bubble/stall a pipeline stage.
 * Call it in handle_hazards(). Do not modify this code. */
void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {
    pipe_reg_t *pipe;
    switch(stage) {
        case S_FETCH: pipe = F_instr; break;
        case S_DECODE: pipe = D_instr; break;
        case S_EXECUTE: pipe = X_instr; break;
        case S_MEMORY: pipe = M_instr; break;
        case S_WBACK: pipe = W_instr; break;
        default: printf("Error: incorrect stage provided to pipe control.\n"); return;
    }
    if (bubble && stall) {
        printf("Error: cannot bubble and stall at the same time.\n");
        pipe->ctl = P_ERROR;
    }
    // If we were previously in an error state, stay there.
    if (pipe->ctl == P_ERROR) return;

    if (bubble) {
        pipe->ctl = P_BUBBLE;
    }
    else if (stall) {
        pipe->ctl = P_STALL;
    }
    else { 
        pipe->ctl = P_LOAD;
    }
}
// This function checks if there is a mispredicted branch hazard.
// It takes in the current instruction opcode X_opcode and the condition code value X_condval.
// It returns true if the current instruction is a conditional branch instruction and the condition code is not valid (!X_condval), which would indicate a mispredicted branch hazard.
bool check_ret_hazard(opcode_t D_opcode) {

    return D_opcode == OP_RET;


}
// This function checks for a load-use hazard.
//  It takes in the opcode of the current instruction in the decode stage D_opcode, source register 1 and 2 D_src1 and D_src2, opcode of the current instruction in the execute stage X_opcode, and the destination register of the current instruction in the execute stage X_dst.
// It returns true if the current instruction is a load instruction and the destination register X_dst matches with either of the source registers D_src1 or D_src2.
bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {
    

    return (X_opcode == OP_B_COND && !X_condval);
}

// This function checks for a return hazard. It takes in the opcode of the current instruction in the decode stage D_opcode.
// It returns true if the current instruction is a RET instruction.
bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                           opcode_t X_opcode, uint8_t X_dst) {
        
    return (X_opcode == OP_LDUR && (D_src1 == X_dst || D_src2 == X_dst));
}

// Function to handle hazards during pipeline execution
// Parameters:
// D_opcode - opcode of the current instruction in the decode stage
// D_src1 - register index of source register 1 for the current instruction in the decode stage
// D_src2 - register index of source register 2 for the current instruction in the decode stage
// X_opcode - opcode of the current instruction in the execute stage
// X_dst - register index of the destination register for the current instruction in the execute stage
// X_condval - condition value for branch instructions in the execute stage
comb_logic_t handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2, 
                            opcode_t X_opcode, uint8_t X_dst, bool X_condval) {
    /* Students: Change this code */


    // Check for return hazard
    if(check_ret_hazard(D_opcode)){
           // Stall pipeline by flushing fetch and decode stages
        // Set execute stage to stop and wait for W stage to complete
        pipe_control_stage(S_FETCH, false, false);
        pipe_control_stage(S_DECODE, true, false);
        pipe_control_stage(S_EXECUTE, false, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    // Check for load-use hazard
    }  else if(check_load_use_hazard(D_opcode, D_src1, D_src2, X_opcode, X_dst)){
        // Stall pipeline by flushing fetch and decode stages
        // Set execute stage to start from the beginning
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, true, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);
    } 
 // Check for mispredicted branch hazard
    else if(check_mispred_branch_hazard(X_opcode, X_condval)){
        // Stall pipeline by flushing fetch and decode stages
        // Set execute stage to start from the beginning
        pipe_control_stage(S_FETCH, false, false);
        pipe_control_stage(S_DECODE, true, false);
        pipe_control_stage(S_EXECUTE, true, false);
        pipe_control_stage(S_MEMORY, false, false);
        pipe_control_stage(S_WBACK, false, false);

}else {
    // No hazard detected, continue pipeline execution as normal
     pipe_control_stage(S_FETCH, false, false);
     pipe_control_stage(S_DECODE, false, false);
     pipe_control_stage(S_EXECUTE, false, false);
     pipe_control_stage(S_MEMORY, false, false);
     pipe_control_stage(S_WBACK, false, false);
    }
  }

