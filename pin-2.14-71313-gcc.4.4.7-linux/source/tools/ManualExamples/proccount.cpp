#include <stdio.h>
#include <math.h>
#include "pin.H"

long num_instr = 0;
const long NUM_INSTR_SKIPPED = 1000000;

//Tournament Components
const int NUM_PREDICTORS = 3;
int individual_predictions[NUM_PREDICTORS];

//Correlated Predictor Components
const int BHT_BITS = 2; // set the number of bits to use for the BHT (1 disables this)
const int BHR_BITS = 12; // set the number of bits to use for the BHR (1 disables this)
const int SATURATING_NUM = (1<<BHT_BITS)/2;
const int BHR_ENTRIES = 1<<BHR_BITS;
int correlated_bhr[BHR_ENTRIES];
int correlated_perceptron_current_index;
bool correlated_history[BHR_BITS]; // stores the current recent history of branch results

//Perceptron Components
const int PERCEPTRON_GEHL_HISTORY_LENGTH = 62; // set the number of bits to use for branch history
const float perceptron_GEHL_theta = (1.93 * PERCEPTRON_GEHL_HISTORY_LENGTH) + 14.0;
const int PERCEPTRON_GEHL_TABLE_SIZE = 1<<10 ; // size of each table
float weight[PERCEPTRON_GEHL_TABLE_SIZE][PERCEPTRON_GEHL_HISTORY_LENGTH];  //storage for weights
bool perceptron_branch_history[PERCEPTRON_GEHL_HISTORY_LENGTH]; // stores the current recent history of branch results
string address_history[PERCEPTRON_GEHL_TABLE_SIZE];
float current_perceptron_result;
int perceptron_current_index = 0;
int perceptron_paths_used = 0;

//GEHL Components
const int COUNTER_BITS = 4; // set the number of bits to use for the counter
const int GEHL_NUM_TABLES = 6; //set the number of predictor tables to utilize
const int GEHL_TABLE_SIZE = 1<<20 ; // size of each table
const int GEHL_HISTORY_LENGTH = 1<<(GEHL_NUM_TABLES-1); // set the number of bits to use for branch history (determined using a=2)
const float GEHL_theta = GEHL_NUM_TABLES;
int GEHLpredictionTable[GEHL_NUM_TABLES][GEHL_TABLE_SIZE]; //Prediction Table
int historyVariations[GEHL_NUM_TABLES]; //Variations on depth per table
string GEHL_branch_history[GEHL_HISTORY_LENGTH]; // stores the current recent history of branch results
bool wasCorrect; //Was the prediction correct?
float GEHLsum;

//System Components
FILE * trace;
bool prediction;
int num_branches;
int num_correct;

//hash function
unsigned long hashing(const char *str)
    {
        unsigned long hash = 5381;
        int c;

        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash/(LLONG_MAX/(1<<19));
    }
	
//Creating and Initilizing Predictor Tables
void createPredictorTables(){
	for(int i=0; i<GEHL_NUM_TABLES;i++){
		for(int j=0; j<GEHL_TABLE_SIZE; j++){
			GEHLpredictionTable[i][j] = 0;
		}
		historyVariations[i] = 1<<i;
	}
}

float boolToFloat(bool input){
    float temp = -1.0;
    if (input) { temp = 1.0; }
    return temp;
}

void updateIndex(VOID* addr){
	//Correlated Predictor
	int index = 0;
    for (int i=0; i<BHR_BITS-1; i++) {
        if (correlated_history[i]){
            index = index | (1<<i);
        }
    }
    correlated_perceptron_current_index = index;
	
	//Perceptron 
	// set index based on hash
    char buffer [100];
	sprintf(buffer,"%p", addr);
	std::string str1 (buffer);
	bool found = false;
	for (int i=0; i<PERCEPTRON_GEHL_TABLE_SIZE; i++){
		if(str1.compare(address_history[i]) == 0){
			perceptron_current_index = i;
			found = true;
			break;
		}
	}
	
	if(!found){
		if(perceptron_paths_used == PERCEPTRON_GEHL_TABLE_SIZE){
			perceptron_paths_used = 0;
		}
		address_history[perceptron_paths_used] = str1;
		perceptron_current_index = perceptron_paths_used;
		perceptron_paths_used += 1;
	}
}

void setPrediction(){
	//Set Correlated Prediction
	if (correlated_bhr[correlated_perceptron_current_index] > 0){
        individual_predictions[0]  = 1;
    }
    else
    {
        individual_predictions[0]  = -1;
    }
	
	//Set Perceptron Prediction
	current_perceptron_result = weight[perceptron_current_index][0];
    for (int i=1; i<PERCEPTRON_GEHL_HISTORY_LENGTH; i++){
        current_perceptron_result += weight[perceptron_current_index][i] * boolToFloat(perceptron_branch_history[i]);
    }
	if (current_perceptron_result > 0.0){
        individual_predictions[1]  = 1;
    }
    else
    {
        individual_predictions[1]  = -1;
    }
	
	//SET GEHL Prediction
	GEHLsum = GEHL_NUM_TABLES/2.0;
    for (int i=0; i<GEHL_NUM_TABLES; i++){
		int historyDepth = historyVariations[i];
		string currentPath;
		for (int j=0; j<historyDepth; j++){
				currentPath = currentPath + GEHL_branch_history[GEHL_HISTORY_LENGTH-(j+1)];
				
		}
		//int hash = atoi(currentPath.c_str());
		unsigned long hash = hashing(currentPath.c_str());
		//printf("%lu\n", hash);
		GEHLsum += GEHLpredictionTable[i][hash];
		//printf("%f\n", GEHLsum);
    }
	if (GEHLsum > 0.0){
        individual_predictions[2]  = 1;
    }
    else
    {
        individual_predictions[2]  = -1;
    }
	
	//Set Actual Prediction
	int sum = 0;
	for (int i=0; i<NUM_PREDICTORS; i++){
		sum += individual_predictions[i];
	}
    if (sum > 0){
        prediction  = true;
    }
    else
    {
        prediction  = false;
    }
}

void updateHistory(bool taken, string address){
	//Update Correlated History
	for (int i=0; i<BHR_BITS-1; i++) {
        correlated_history[i] = correlated_history[i+1];
    }
    correlated_history[BHR_BITS-1] = taken;
	
	//Update Perceptron History
	perceptron_branch_history[0] = taken;
    for (int i=(PERCEPTRON_GEHL_HISTORY_LENGTH-1); i>1; i++) {
		perceptron_branch_history[i] = perceptron_branch_history[i-1];
    }
    perceptron_branch_history[0] = true;
	
	//Update GEHL History
	for (int i=0; i<GEHL_HISTORY_LENGTH-1; i++) {
        GEHL_branch_history[i] = GEHL_branch_history[i+1];
    }
    GEHL_branch_history[GEHL_HISTORY_LENGTH-1] = address;	
}

void updatePredictors(bool taken, bool wasCorrect){
	//Update Correlated 
	if (taken)
    {
        if (correlated_bhr[correlated_perceptron_current_index] == -1) correlated_bhr[correlated_perceptron_current_index]++;
        correlated_bhr[correlated_perceptron_current_index]++;
    }
    else
    {
        if (correlated_bhr[correlated_perceptron_current_index] == 1) correlated_bhr[correlated_perceptron_current_index]--;
        correlated_bhr[correlated_perceptron_current_index]--;
    }
    correlated_bhr[correlated_perceptron_current_index] = std::max(correlated_bhr[correlated_perceptron_current_index], -BHT_BITS);
    correlated_bhr[correlated_perceptron_current_index] = std::min(correlated_bhr[correlated_perceptron_current_index], BHT_BITS);
	
	//Update Perceptron
	float temp_result = current_perceptron_result;
    if (temp_result < 0.0) {
		temp_result *= -1.0;
	}
    
    if ((prediction != taken) || (temp_result <= perceptron_GEHL_theta)){
        for (int i=0; i<PERCEPTRON_GEHL_HISTORY_LENGTH; i++){
            weight[perceptron_current_index][i] = weight[perceptron_current_index][i] + boolToFloat(taken)*boolToFloat(perceptron_branch_history[i]);
        }
    }
	
	//Updated GEHL
 	if(GEHLsum<=GEHL_theta || !wasCorrect){
		for (int i=0; i<GEHL_NUM_TABLES; i++){
			int historyDepth = historyVariations[i];
			string currentPath;
			for (int j=0; j<historyDepth; j++){
				currentPath = currentPath + GEHL_branch_history[GEHL_HISTORY_LENGTH-(j+1)];
				
			}
		unsigned long hash = hashing(currentPath.c_str());
		
		if(taken){
			GEHLpredictionTable[i][hash]+=1;
			GEHLpredictionTable[i][hash] = std::min(GEHLpredictionTable[i][hash], COUNTER_BITS);
		}
		else{
			GEHLpredictionTable[i][hash]+=(-1);
			GEHLpredictionTable[i][hash] = std::max(GEHLpredictionTable[i][hash], -COUNTER_BITS);
			//printf("False");
		}
		if(GEHLpredictionTable[i][hash] > COUNTER_BITS){
			printf("%i\n",GEHLpredictionTable[i][hash]);
		}
		}
	}		
	
}

// Print a branch record
VOID RecordBranch(VOID * ip, BOOL taken, VOID * addr)
{
    fprintf(trace,"%p: %p", ip, addr);
    num_branches++;
	updateIndex(addr);
    setPrediction();
	
    if (taken)
    {
        fprintf(trace,"\tT");
        if (prediction) {
			wasCorrect = true;
            num_correct++;
            fprintf(trace," ");
        }
        else
        {
			wasCorrect = false;
            fprintf(trace,"*");
        }
    }
    else
    {
        fprintf(trace,"\tN");
        if (!prediction) {
			wasCorrect = true;
            num_correct++;
            fprintf(trace," ");
        }
        else
        {
			wasCorrect = false;
            fprintf(trace,"*");
        }
    }
    fprintf(trace," %+d\tcorrect=%d/%d\n", 0, num_correct, num_branches);
	
	updatePredictors(taken,wasCorrect);
  	char buffer [100];
	sprintf(buffer,"%p", addr);
	std::string str1 (buffer);
    updateHistory(taken, str1);
    
}

// This function is called before every instruction is executed
VOID docount() { num_instr++; }

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Insert a call to docount before every instruction, no arguments are passed
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);
    if (num_instr <= NUM_INSTR_SKIPPED){
        num_correct = 0;
        num_branches = 0;
    }
    if (INS_IsBranch(ins))
    {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordBranch,
            IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_BRANCH_TARGET_ADDR, 
            IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace,"...\nOVERALL correct=%d/%d\t%f using a tournament predictor.\n", 
        num_correct, num_branches, (double)num_correct/(double)num_branches);
    fprintf(trace, "num_instr=%lu\n", num_instr);
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

    trace = fopen("branchlog.out", "w");
	
	createPredictorTables();
	
	for(int i=0; i<NUM_PREDICTORS; i++){
		individual_predictions[i] = 1;
	}
    
    num_branches = 0;
    num_correct = 0;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}