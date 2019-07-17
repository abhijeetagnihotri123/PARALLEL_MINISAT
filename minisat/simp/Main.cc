/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <errno.h>
#include <zlib.h>
#include <mpi.h>
#include <vector>
#include "minisat/utils/System.h"
#include "minisat/utils/ParseUtils.h"
#include "minisat/utils/Options.h"
#include "minisat/core/Dimacs.h"
#include "minisat/core/Solver.h"
using namespace Minisat;

//=================================================================================================


static Solver* solver;
static Solver* solver1;
static Solver* solver2;
static Solver* solver3;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int) { solver->interrupt(); }
static void SIGINT_interrupt1(int) { solver1->interrupt(); }
static void SIGINT_interrupt2(int) { solver2->interrupt(); }
static void SIGINT_interrupt3(int) { solver3->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver->verbosity > 0){
        solver->printStats();
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }
static void SIGINT_exit1(int) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver1->verbosity > 0){
        solver1->printStats();
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }
static void SIGINT_exit2(int) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver2->verbosity > 0){
        solver2->printStats();
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }
static void SIGINT_exit3(int) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver3->verbosity > 0){
        solver3->printStats();
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }

//=================================================================================================
// Main:


int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        setX86FPUPrecision();
        int id,size; // variables to hold rank of the processes and size hold the number of processes
        MPI_Init(&argc,&argv);
        MPI_Comm_size(MPI_COMM_WORLD,&size); // get number of processes
        MPI_Comm_rank(MPI_COMM_WORLD,&id); // get rank of process
        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", 0, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", 0, IntRange(0, INT32_MAX));
        BoolOption   strictp("MAIN", "strict", "Validate DIMACS header during parsing.", false);
        
        parseOptions(argc, argv, true);

        Solver S,S1,S2,S3; // introduce four different solver objects one for each process as we have four processes
        double initial_time = cpuTime();

        S.verbosity = verb;
        S1.verbosity = verb;
        S3.verbosity = verb;
        S2.verbosity = verb;
        solver = &S;
        solver1 = &S1;
        solver2 = &S2;
        solver3 = &S3;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        sigTerm(SIGINT_exit);

        // Try to set resource limits:
        if (cpu_lim != 0) limitTime(cpu_lim);
        if (mem_lim != 0) limitMemory(mem_lim);
        
        if (argc == 1)
            printf("Reading from standard input... Use '--help' for help.\n");
        //opening the file pointer for individual file pointers
        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb"); 
        gzFile in1 = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        gzFile in2 = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        gzFile in3 = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        if (in == NULL)
            printf("ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
        
        if (S.verbosity > 0){
            printf("============================[ Problem Statistics ]=============================\n");
            printf("|                                                                             |\n"); }
        // parse the CNF expression for each of the solver objects
        parse_DIMACS(in, S, (bool)strictp); 
        parse_DIMACS(in1, S1, (bool)strictp);       
        parse_DIMACS(in2, S2, (bool)strictp);
        parse_DIMACS(in3, S3, (bool)strictp);
        gzclose(in);
        gzclose(in1);
        gzclose(in2);
        gzclose(in3);
        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        //if (S.verbosity > 0){
        //    printf("|  Number of variables:  %12d                                         |\n", S.nVars());
        //    printf("|  Number of clauses:    %12d                                         |\n", S.nClauses()); }
        
        double parsed_time = cpuTime();
        //if (S.verbosity > 0){
        //    printf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);
        //    printf("|                                                                             |\n"); }
 
        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        sigTerm(SIGINT_interrupt);
        sigTerm(SIGINT_interrupt1);
        sigTerm(SIGINT_interrupt2);
        sigTerm(SIGINT_interrupt3);
        if (!S.simplify()){
            if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
            if (S.verbosity > 0){
                printf("===============================================================================\n");
                printf("Solved by unit propagation\n");
                //S.printStats();
                printf("%d %d\n",size,id); }
            printf("UNSATISFIABLE\n");
            exit(20);
        }
        if (!S1.simplify()){
            if (S1.verbosity > 0){
                printf("===============================================================================\n");
                printf("Solved by unit propagation\n");
                //S1.printStats();
                printf("%d %d\n",size,id); }
            printf("UNSATISFIABLE\n");
            exit(20);
        }
        if (!S2.simplify()){
            if (S2.verbosity > 0){
                printf("===============================================================================\n");
                printf("Solved by unit propagation\n");
                //S2.printStats();
                printf("%d %d\n",size,id); }
            printf("UNSATISFIABLE\n");
            exit(20);
        }
        if (!S3.simplify()){
            if (S.verbosity > 0){
                printf("===============================================================================\n");
                printf("Solved by unit propagation\n");
                //S.printStats();
                printf("%d %d\n",size,id); }
            printf("UNSATISFIABLE\n");
            exit(20);
        }
        //use mpi here
        int x=1; // if x and y are unit clauses the the CNF becomes (if C is the input CNF from dimacs file)
        bool sign=true;//then it becomes C^x^y for process with rank 0
        Lit p = mkLit(x,sign);
        vec<Lit>dummy;
        vec<Lit>dummy1;
        vec<Lit>dummy2;
        vec<Lit>dummy3;
        S.addClause(p);
        x=2;
        p = mkLit(x,sign);
        S.addClause(p);
        x=1;
        sign=true;
        p=mkLit(x,sign);
        S1.addClause(p);
        x=2;
        sign=false;
        p=mkLit(x,sign);
        S1.addClause(p);
        x=1;
        sign=false;
        p=mkLit(x,sign);
        S2.addClause(p);
        x=2;
        sign=true;
        p=mkLit(x,sign);
        S2.addClause(p);
        sign=false;
        x=1;
        p=mkLit(x,sign);
        S3.addClause(p);
        x=2;
        p=mkLit(x,sign);
        S3.addClause(p);
        lbool ret,ret1,ret2,ret3,ret4;
        if(id==0)
        { 
            ret1=S.solveLimited(dummy);
            int res;
            if(S.verbosity > 0)
            {
                printf("Process with id %d\n",id);
            }
            if(ret1==l_True)
            {
                res=0;
            }
            else if(ret1==l_False)
            {
                res=1;
            }
            else
            {
                res=2;
            }
            MPI_Send(&res,1,MPI_INT,3,0,MPI_COMM_WORLD);
        }
        else if(id==1)
        {
            ret2=S1.solveLimited(dummy1);
            int res;
            if(S1.verbosity > 0)
            {
                printf("Process with id %d\n",id);
            }
            if(ret2==l_True)
            {
                res=0;
            }
            else if(ret2==l_False)
            {
                res=1;
            }
            else
            {
                res=2;
            }
            MPI_Send(&res,1,MPI_INT,3,0,MPI_COMM_WORLD);
        }
        else if(id==2)
        {
            ret3=S2.solveLimited(dummy2);
            int res;
            if(S2.verbosity > 0)
            {
                printf("Process with id %d\n",id);
            }
            if(ret3==l_True)
            {
                res=0;
            }
            else if(ret3==l_True)
            {
                res=1;
            }
            else
            {
                res=2;
            }
            MPI_Send(&res,1,MPI_INT,3,0,MPI_COMM_WORLD);
        }
        else if(id==3)
        {
            ret4=S3.solveLimited(dummy3);
            int res[4];
            if(S3.verbosity > 0)
            {
                printf("Process with id %d\n",id);
            }
            if(ret4==l_True)
            {
                res[3]=0;
            }
            else if(ret4==l_False)
            {
                res[3]=1;
            }
            else
            {
                res[3]=2;
            }
            for(int i=0;i<=2;i++)
            {
                MPI_Recv((res+i),1,MPI_INT,i,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
            }
            if(res[0]==0 || res[1]==0 || res[2]==0 || res[3]==0)
            {
                ret=l_True;
            }
            else if(res[0]==1 || res[1]==1 || res[2]==1 || res[3]==1)
            {
                ret=l_False;
            }
            printf("%d %d %d %d\n",res[0],res[1],res[2],res[3]);//flags for each of the solver objects of respective process
            if(ret==l_True) //0 is satisfiable 1 for unsatisfiable and 2 for indeterminate
            {
                printf("SATISFIABLE\n");
            }
            else if(ret==l_False)
            {   
                printf("UNSATISFIABLE\n");
            }
            else
            {
                printf("INDETERMINATE\n");
            }
            printf("MPI used 4 processes to search for a satisfying solution with different variables\n");
            printf("This process is the master process which receives the results from other process\n");
        }
            //printf("Number of processes %d ",size);
            if (res != NULL){
                if (ret1 == l_True){
                    fprintf(res, "SAT\n");
                    for (int i = 0; i < S.nVars(); i++)
                        if (S.model[i] != l_Undef)
                            fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
                    fprintf(res, " 0\n");
                }else if (ret1 == l_False)
                    fprintf(res, "UNSAT\n");
                else
                    fprintf(res, "INDET\n");
                fclose(res);
            }
        fflush(stdout);
        MPI_Finalize();
#ifdef NDEBUG
        return 0;
        //exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
#endif
    } catch (OutOfMemoryException&){
        printf("===============================================================================\n");
        printf("INDETERMINATE\n");
        exit(0);
    }
}