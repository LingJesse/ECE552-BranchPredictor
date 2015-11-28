#include <stdio.h>
#include "pin.H"


FILE * trace;

// Print a branch record
VOID RecordBranchTaken(VOID * ip, BOOL taken, VOID * addr)
{
    if (taken)
    {
        fprintf(trace,"%p:     TAKEN %p\n", ip, addr);
    }
    else
    {
        fprintf(trace,"%p: NOT TAKEN %p\n", ip, addr);
    }
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    if (INS_IsBranch(ins))
    {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordBranchTaken,
            IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_BRANCH_TARGET_ADDR, 
            IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of branch instructions\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("branchlog.log", "w");

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
