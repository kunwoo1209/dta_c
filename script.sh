#!/bin/bash
PROGRAM_NAME="dummy1" # need to set
PROGRAM_FILE="dummy1_pre.c" # need to set (should be pre-processed)


# Step1: generate binary file of instrumented target program
./instrumentor $PROGRAM_FILE
gcc output.c -o $PROGRAM_NAME

# Step2: generate a text file that contains list of functions 
./list_function $PROGRAM_FILE >"$PROGRAM_NAME"_functions

# Step3: run tc and generate csv file for each tc in the directory called "function_correlation"
mkdir -p function_correlation
########## need to set ##########
### Example: executing 3 tcs ####
# tc1
./$PROGRAM_NAME
python duchain.py $PROGRAM_NAME 1
# tc2
./$PROGRAM_NAME
python duchain.py $PROGRAM_NAME 2
# tc3
./$PROGRAM_NAME
python duchain.py $PROGRAM_NAME 3

# Step4: combine all the generated csv files in a single csv file called "[PROGRAM_NAME]_function_correlation.csv"
python combine_result.py $PROGRAM_NAME
