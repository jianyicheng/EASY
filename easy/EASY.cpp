#include "EASY.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Use.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
//#include "../../IR/ConstantsContext.h"
//#include "llvm/IR/ConstantRange.h"
//#include "utils.h"
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include "llvm/Support/raw_ostream.h"

#include "Slice.h"
#include "CFSlice.h"
#include "ISlice.h"
#include "LoadRunaheadAnalysis.h"
#include "MemSlice.h"
#include "SliceArrayRefs.h"
#include "SliceCriterion.h"
#include "SliceHelpers.h"
#include "SliceMem.h"

#include "llvm/IR/Dominators.h"

using namespace llvm;

namespace EASY {
    /*
     bool printAPF = true;
     std::string FileError;
     raw_fd_ostream apf("partition.legup.rpt", FileError, llvm::sys::fs::F_None);
     std::string FileError2;
     raw_fd_ostream debugIRFile("IR.legup.ll", FileError2, llvm::sys::fs::F_None);
     */
    bool EASY::runOnModule(Module &M)
    {
        //initialization();
        Mod = &M;
        partition_flag = 0;
        bpl.open ("op.bpl", std::fstream::out);
        errs() << "\nBegin Analysis: \n";
        
        // Step 0: Extract of function threads
        analyzeThreadInfo(M);
        // Step 1: Extraction of partitioned array
        findPartitionedArrays(M);
        // Step 2: Extraction of global memory accesses in threadfunction
        sliceThreadFunction(M);
        
        // Here is assumed that the input arguements of thread function are preprocessed to constant in IR code
        // Step 3: LLVM IR Code to Boogie Conversion
        interpretToBoogie(M);
        
        // Step 3.5: LLVM IR Code to C simulator
        interpretToCSimulator(M);
        
        bpl.close();
        errs() << "\n---------------- Transform to Boogie Program Finished ----------------\n";
        
        return false;
    }

    // Translate IR code to Boogie
    void EASY::interpretToBoogie(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "4. Converting LLVM IR Code to Boogie Code...\n";
        errs() << "*********************************************\n";
        
        bpl << "\n// Datatype conversion from bool to bv32\n";
        bpl << "procedure {:inline 1} bool2bv32 (i: bool) returns ( o: bv32)\n";
        bpl << "{\n";
        bpl << "\tif (i == true)\n";
        bpl << "\t{\n";
        bpl << "\t\to := 1bv32;\n";
        bpl << "\t}\n";
        bpl << "\telse\n";
        bpl << "\t{\n";
        bpl << "\t\to := 0bv32;\n";
        bpl << "\t}\n";
        bpl << "}\n";

        
        // search phi instructions for insertion
        anlyzePhiInstr(M);
        
        // get all pointer values (interprocedural)
        for (auto F = M.begin(); F != M.end(); ++F) {    // Function level
            if (F->size() != 0)                          // ignore all empty functions
            {
                if (F->getName() == "main")                
                {
                	bpl << "\n// For function: " << static_cast<std::string>((F->getName()));
                	// print function prototypes
                	printFunctionPrototype(F, &bpl);
                    // just print thread function information in main
                    printVarDeclarationsInMain(F, &bpl);
                    bpl << "\n";
                    instrDecodingInMain(F, &bpl);
                    bpl << "}\n";   // indicate end of function
                }
                else if ((Function *)F == threadFunc)
                {
                	bpl << "\n// For function: " << static_cast<std::string>((F->getName()));
                	// print function prototypes
                	printFunctionPrototype(F, &bpl);
                    // variable definitions
                    printVarDeclarations(F, &bpl);
                    bpl << "\n";
                    // decode all instructions
                    instrDecoding(F, &bpl);
                	bpl << "}\n";   // indicate end of function
                }
                            
                
                // errs() << "check1\n";
            }
            else
                errs() << "Function: " << static_cast<std::string>((F->getName())) << "is empty so ignored in Boogie\n";
        }                                                                       // End of function level analysis
        // errs() << "check\n";
        errs() << "\nTransfering to Boogie finished. \n";
    }   // end of interpretToBoogie
    
    // print argument of the function in prototype
    void EASY::printFunctionPrototype(Function *F, std::fstream *fout){
        if (static_cast<std::string>((F->getName())) == "main")
            // main function prototype
            *fout << "\nprocedure main() \n";
        else if ((Function *)F == threadFunc)
        {
            // other thread functions
            *fout << "\nprocedure {:inline 1} " << static_cast<std::string>((F->getName())) << "(";
            if (!(F->arg_empty()))
            {
                for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
                {
                    *fout << printRegNameInBoogie(fi) << ": bv32";
                    
                    auto fi_comma = fi;
                    fi_comma++;
                    if (fi_comma != F->arg_end())
                        *fout << ", ";
                }
            }
            // only returns the bank address information
            *fout << ") returns (";
            for (unsigned int i = 0; i < bankAddressInstrLsit.size(); i++)
            {
                *fout << "read_" << i << ": bool, index_" << i << ": bv32";
                if (i != bankAddressInstrLsit.size() - 1)
                    *fout << ", ";
            }
            *fout << ") \n";
        }
        else
        {
        	// other thread functions
            *fout << "\nprocedure {:inline 1} " << static_cast<std::string>((F->getName())) << "(";
            if (!(F->arg_empty()))
            {
                for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
                    *fout << printRegNameInBoogie(fi) << ": bv32, ";
          		
				// note: loop index can be more than one
				*fout << "store_input: bv32";
            }
            // only returns the bank address information
            *fout << ") returns (array_data: bv32) \n";
        }
        
        // add modifies - listed array name
        for (auto it = arrayInfo.begin(); it != arrayInfo.end(); ++it)
        {
            arrayNode *AN = *it;
            std::string arrayName;
            arrayName = AN->name;
            *fout << "modifies @" << arrayName << ";\n";
        }
        
        *fout << "{\n";
    } // end of printFunctionPrototype
    
    // This section is to collect all the variables have been used and prepare for declaration in Boogie
    void EASY::printVarDeclarationsInMain(Function *F, std::fstream *fout)
    {
        for (int i = 0; i < threadNum; i++)
        {
            for (unsigned int j = 0; j < bankAddressInstrLsit.size(); j++)
                *fout << "\tvar t" << i << "_read_" << j << ": bool" <<";\n\tvar t" << i << "_index_" << j << " :bv32;\n";
        }
    }   //  end of printVarDeclarationsInMain
    
    // This section is to collect all the variables have been used and prepare for declaration in Boogie
    void EASY::printVarDeclarations(Function *F, std::fstream *fout)
    {
        for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                switch (I->getOpcode()) {
                    case 1:     // ret
                        // no need for var declarations
                        break;
                        
                    case 2:     // br
                        if (I->getNumOperands() == 3)
                        {
                            if (!varFoundInList((Value *)(I->getOperand(0)), F))
                                varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        }
                        break;
                        
                    case 8:     // add
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                        
                    case 10:     // sub
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 12:     // mul
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 20:     // shl
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 21:     // lshr
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 22:     // ashr
                        partition_flag = 1;
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 23:     // and
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 24:     // or
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    
                    case 27:     // load
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        break;
                        
                    case 28:     // store
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    
                    case 29:     // getelementptr                               // this is what we want to modify
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);     // leave output random
                        break;
                        
                    case 33:     // load
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        break;
                        
                    case 34:     // zext
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        break;
                        
                    case 46:    // icmp
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 1);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 48:     // phi
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        break;
                    case 49:     // call
                        if (I->getNumOperands() > 1)
                        {
                            for (unsigned int i = 0; i != (I->getNumOperands()- 1); ++i)
                            {
                                if (!varFoundInList((Value *)(I->getOperand(i)), F))
                                    varDeclaration((Value *)(I->getOperand(i)), fout, 0);
                            }
                        }
                        break;
                    case 50:     // select
                        partition_flag = 1;
                        if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 1);
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(1)), F))
                            varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                        if (!varFoundInList((Value *)(I->getOperand(2)), F))
                            varDeclaration((Value *)(I->getOperand(2)), fout, 0);
                        break;
                        
                    default:
                        ;
                }
                
            }                                                               // End of instruction level analysis
        }                                                                   // End of basic block level analysis
    }
    
    // To split phi instruction to several load in corresponding basic blocks
    void EASY::anlyzePhiInstr(Module &M)
    {
        for (auto F = M.begin(); F != M.end(); ++F) {   // Function level
            for (auto BB = F->begin(); BB != F->end(); ++BB) {  // Basic block level
                for (auto I = BB->begin(); I != BB->end(); ++I) {   // Instruction level
                    if (I->getOpcode() == 48)       // phi instruction
                    {
                        if (llvm::PHINode *phiInst = dyn_cast<llvm::PHINode>(&*I)) {
                            phiNode *phiTfInst = new phiNode [phiInst->getNumIncomingValues()];
                            
                            for (unsigned int it = 0; it < phiInst->getNumIncomingValues(); ++it)
                            {
                                phiTfInst[it].res = printRegNameInBoogie(I);
                                phiTfInst[it].bb = phiInst->getIncomingBlock(it);
                                phiTfInst[it].ip = phiInst->getIncomingValue(it);
                                phiTfInst[it].instr = phiInst;
                                
                                phiList.push_back(&phiTfInst[it]);
                            } // end for
                        }  // end if
                    } // end if
                }  // End of instruction level
            }   // End of basic block level
        }   // End of function level
    }   // end of anlyzePhiInstr
    
    // Call function printing in main functions
    void EASY::instrDecodingInMain(Function *F, std::fstream *fout)
    {
        int t_i = 0;
        // print call instructions
        for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                if (I->getOpcode() == 49) {
                    if (I->getOperand(I->getNumOperands()-1) == threadFunc)
                    {
                        *fout << "\t// thread function call " << t_i << ": \n";
                        *fout << "\tcall ";
                        for (unsigned int idxit = 0; idxit < bankAddressInstrLsit.size(); idxit++)
                        {
                            *fout << "t" << t_i << "_read_" << idxit <<", t" << t_i << "_index_" << idxit;
                            if (idxit != bankAddressInstrLsit.size() - 1)
                                *fout << ", ";
                        }
                        *fout <<" := " << static_cast<std::string>((I->getOperand(I->getNumOperands()-1))->getName()) << "(";
                        if (I->getNumOperands() > 1)
                        {
                            *fout << printRegNameInBoogie((Value *)I->getOperand(0));
                            for (unsigned int i = 1; i != (I->getNumOperands()- 1); ++i)
                                *fout << "," << printRegNameInBoogie((Value *)I->getOperand(i));
                        }
                        *fout << ");\n";
                        t_i++;
                    }   // end if
                }   // end if
            }      // End of instruction level
        }// End of basic block level
        
        for (int i = 0; i < threadNum; i++)
        {
            for (unsigned int k = 0; k < bankAddressInstrLsit.size(); k++)
            {
                for (int j = 0; j < partitionNum; j++)
                    *fout << "\tassert !t" << i << "_read_" << k << " || t" << i << "_index_" << k << " != " << j << "bv32;\n";
            }
        }
        
        *fout << "\n\treturn;\n";
    }   // end of instrDecodingInMain
    
    
    // Translate all IR instruction to Boogie instruction
    void EASY::instrDecoding(Function *F, std::fstream *fout)
    {
        // get all loop information
        llvm::DominatorTree* DT = new llvm::DominatorTree();
        DT->recalculate(*F);
        //generate the LoopInfoBase for the current function
        llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>* KLoop = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
        KLoop->releaseMemory();
        KLoop->Analyze(*DT);
        for (unsigned int i = 0; i < bankAddressInstrLsit.size(); i++)
        {
            *fout << "\tread_" << i << " := false;\n";
        }
        
        int indexCounter = 0;
        for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
            // *fout << "// For basic block: " << BB->getName() << "\n";
            *fout << "\n\t// For basic block: bb_" << getBlockLabel(BB) << "\n";
            *fout << "\tbb_" << getBlockLabel(BB) << ":\n";
            // errs() << "debug: " << *BB << "\n";
            // Here add assertion of loop invariant conditions at start of the loop
            if(KLoop->isLoopHeader(BB))
            {
                // phiNode *phiInst;
                // unsigned int idxNameAddr;
                if (BasicBlock *BBExit = KLoop->getLoopFor(BB)->getExitingBlock())
                {
                    Instruction *exitCond;
                    Instruction *startCond;
                    Instruction *indVarBehaviour;
                    std::string endBound;
                    std::string startBound;
                    std::string indvarName;
                    std::string endSign;
                    std::string startSign;
                    std::string loopInverseCheck;   // check if exitcondition inversed
                    bool equalSign = 0;
                
                    auto instrexit = BBExit->end();
                    --instrexit;
                    
                    if (printRegNameInBoogie((Value *)instrexit->getOperand(0)) != "undef")
                    {
                        // instruction contains end loop index
                        for (auto exit_i = BBExit->begin(); exit_i != BBExit->end(); ++exit_i)
                        {
                            if (printRegNameInBoogie(exit_i) == printRegNameInBoogie(instrexit->getOperand(0)))
                                exitCond = exit_i;
                        }
                        endBound = printRegNameInBoogie(exitCond->getOperand(1));
                        
                        // check exit equality
                        loopInverseCheck = printRegNameInBoogie(exitCond);
                        loopInverseCheck.resize(9);
                        if (loopInverseCheck == "%exitcond")
                        {
                            if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*exitCond)) {
                                if (cmpInst->getPredicate() == CmpInst::ICMP_NE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_UGT)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_ULT)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SGT)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SLT)
                                    equalSign = 1;
                            }
                        }
                        else
                        {
                            if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*exitCond)) {
                                if (cmpInst->getPredicate() == CmpInst::ICMP_EQ)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_UGE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_ULE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SGE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SLE)
                                    equalSign = 1;
                            }
                        }
                        
                        // instruction contains start loop index
                        // this will not hold for matrix transfer... where exit condition not compatible...
                        
                        for (auto invarI = BBExit->begin(); invarI != BBExit->end(); ++invarI)
                        {
                            if (invarI->getOpcode() != 2)   // label name and variable name can be same causing bugs...
                            {
                                for (auto invarIt = phiList.begin(); invarIt != phiList.end(); ++invarIt)
                                {
                                    phiNode *phiTfInst = *invarIt;
                                    for (unsigned int it = 0; it < invarI->getNumOperands(); ++it)
                                    {
                                        if (phiTfInst->res == printRegNameInBoogie((Value *)invarI->getOperand(it)))
                                        {
                                            if (invarI->getOpcode() == 8 || invarI->getOpcode() == 10)
                                            {
                                                indVarBehaviour = invarI;
                                                startCond = dyn_cast<Instruction>(invarI->getOperand(it));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        if (indVarBehaviour->getOpcode() == 8)    // add
                        {
                            startSign = "bv32sge(";
                            if (equalSign)
                                endSign = "bv32sle(";
                            else
                                endSign = "bv32slt(";
                        }
                        else if (indVarBehaviour->getOpcode() == 10)  // sub
                        {
                            startSign = "bv32sle(";
                            if (equalSign)
                                endSign = "bv32sge(";
                            else
                                endSign = "bv32sgt(";
                        }
                        else
                            errs() <<"Undefined index behaviour: "<< *indVarBehaviour << "\n";
                        
                        
                        indvarName = printRegNameInBoogie(startCond);
                        
                        // start and exit sign
                        if (llvm::PHINode *phiInst = dyn_cast<llvm::PHINode>(&*startCond)) {
                            for (unsigned int it = 0; it < phiInst->getNumIncomingValues(); ++it)
                            {
                                if (ConstantInt *constVar = dyn_cast<ConstantInt>(phiInst->getIncomingValue(it)))
                                    startBound = printRegNameInBoogie(constVar);
                            } // end for
                        }  // end if
                        
                        // construct loop invariant string
                        invariance *currInvar = new invariance;
                        currInvar->loop = KLoop->getLoopFor(BB);
                        currInvar->indVarChange = indVarBehaviour;
                        currInvar->invar = startSign + indvarName + "," + startBound + ") && " + endSign + indvarName + "," + endBound + ")";
                        // debug
                        errs() << "Found loop invariant condition: (" << currInvar->invar << ")\n\n";
                        invarList.push_back(currInvar);
                        
                        // print
                        *fout << "\tassert ( " << currInvar->invar << ");\n";
                        *fout << "\thavoc " << indvarName << ";\n";
                        *fout << "\tassume ( " << currInvar->invar << ");\n";
                    }   // end of check undef
                }
                else
                {
                    // mark - this is the case of while(1)
                    // Due to unfinished automated loop invariants generation, this section will be filled in the future updates
                    errs() << "Found infintite loop entry: " << *BB << "\n";
                    errs() << "Found infintite loop: " << *(KLoop->getLoopFor(BB)) << "\n";
                    
                }
                
            }   // end of insert loop invariants in the beginning
            
            // Here add assertion of loop invariant conditions right before loop index change
            BasicBlock *currBB = BB;
            Instruction *endLoopCheckPoint;
            std::string endLoopInvar;
            int dim = 0;
            int search;
            if(KLoop->getLoopFor(BB))
            {
                if (currBB == KLoop->getLoopFor(BB)->getExitingBlock())
                {
                    auto instrexit = currBB->end();
                    --instrexit;
                    Instruction *instrExit = instrexit;
                    if (printRegNameInBoogie((Value *)instrExit->getOperand(0)) != "undef")
                    {
                        // errs() << "loop exit: " << *BB << "\n";
                        search = 0;
                        for (auto it = invarList.begin(); it != invarList.end(); ++it)
                        {
                            invariance *currInvar = *it;
                            if (KLoop->getLoopFor(BB) == currInvar->loop)
                            {
                                search++;
                                endLoopCheckPoint = currInvar->indVarChange;
                                endLoopInvar = currInvar->invar;
                            }
                        }
                        if (search != 1)
                            errs() << "Error: Loop invariant errors at end of the loop - " << search << "conditions matched.\n";
                    }
                }
            }
            
            
            // Start instruction printing
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                // start printing end loop assertion here
                if ((Instruction *)I == endLoopCheckPoint)
                {
                    errs() << "Matched loop invariant condition " << search << ": (" << endLoopInvar << ")\n\n";
                    *fout << "\tassert ( " << endLoopInvar << ");\n";
                    // skip current basic block
                    I = BB->end();
                    --I;
                    // *fout << "\treturn;\n\tassume false;\n";
                }
                
                switch (I->getOpcode()) {
                    case 1:     // ret
                        *fout << "\treturn;\n";  //default return - memory not accessed.
                        break;
                        
                    case 2:     // br
                        // do phi resolution here
                        for (auto it = phiList.begin(); it != phiList.end(); ++it)
                        {
                            phiNode *phiTfInst = *it;
                            if (phiTfInst->bb == BB)
                                *fout << "\t" << phiTfInst->res << " := " << printRegNameInBoogie(phiTfInst->ip) << ";\n";
                        }
                        
                        // add branch instruction here
                        if (endLoopCheckPoint->getParent() == I->getParent()) {
                            // typically for loop jumping back to iteration
                            // show original in comments
                            if (I->getNumOperands() == 1) // if (inst->isConditional())?
                                errs() << "Error: Mistaken a simple br as a loop exit condition check: " << *I << "\n";
                            else if (I->getNumOperands() == 3)
                                *fout << "//\tif(" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == 1bv32) {goto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(2))) << ";} else {goto bb_" << getBlockLabel((BasicBlock *)(I->getOperand(1))) << ";}\n";
                            else
                                errs() << "Error: Instruction decoding error at br instruction: " << *I << "\n";
                            
                            *fout << "\tgoto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(2))) << ";\n";
                        }
                        else
                        {   // normal cases
                            if (I->getNumOperands() == 1) // if (inst->isConditional())?
                                *fout << "\tgoto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(0))) << ";\n";
                            else if (I->getNumOperands() == 3)
                                *fout << "\tif(" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == 1bv32) {goto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(2))) << ";} else {goto bb_" << getBlockLabel((BasicBlock *)(I->getOperand(1))) << ";}\n";
                            else
                                errs() << "Error: Instruction decoding error at br instruction: " << *I << "\n";
                        }
                        break;
                        
                    case 8:     // add
                        if (OverflowingBinaryOperator *op = dyn_cast<OverflowingBinaryOperator>(I)) {
                            if ((op->hasNoUnsignedWrap()) && (op->hasNoSignedWrap()))
                                // has both nuw and nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoUnsignedWrap())
                                // only nuw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoSignedWrap())
                                // only nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else
                                // normal add
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        }
                        else
                            errs() << "Error: Instruction decoding error at add instruction: " << *I << "\n";
                        break;
                        
                    case 10:     // sub
                        if (OverflowingBinaryOperator *op = dyn_cast<OverflowingBinaryOperator>(I)) {
                            if ((op->hasNoUnsignedWrap()) && (op->hasNoSignedWrap()))
                                // has both nuw and nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoUnsignedWrap())
                                // only nuw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoSignedWrap())
                                // only nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else
                                // normal add
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        }
                        else
                            errs() << "Error: Instruction decoding error at sub instruction: " << *I << "\n";
                        break;
                        
                    case 12:     // mul
                        *fout << "\t" << printRegNameInBoogie(I) << " := bv32mul("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        break;
                    case 20:     // shl
                        *fout << "\t" << printRegNameInBoogie(I) << " := bv32shl("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        break;
                    case 21:     // lshr
                        *fout << "\t" << printRegNameInBoogie(I) << " := bv32lshr("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        break;
                        
                    case 22:     // ashr
                        *fout << "\t" << printRegNameInBoogie(I) << " := bv32ashr("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        break;
                    case 23:     // and
                        *fout << "\t" << printRegNameInBoogie(I) << " := bv32and("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        break;
                    case 24:     // or
                        *fout << "\t" << printRegNameInBoogie(I) << " := bv32or("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        break;
                    case 27:     // load
                        
                        if (isa<GetElementPtrInst>((Instruction *)I->getOperand(0)))
                        {
                            for (auto ait = arrayInfo.begin(); ait != arrayInfo.end(); ++ait)
                            {   // get dimension
                                arrayNode *AN = *ait;
                                std::vector<uint64_t> size = AN->size;
                                GlobalVariable *addr = AN->addr;
                                
                                if (((Instruction *)I->getOperand(0))->getOperand(0) == addr)
                                {
                                    dim = size.size();
                                    break;
                                }
                            }
                            
                            if (dim == 0)
                                errs() << "Error: unknown array stored - cannot find size: " << *I << "\n";
                            else if (dim == 1)
                                *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(2)) << "];\n";
                            else if (dim == 2)
                                *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(1)) << "][" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(2)) << "];\n";
                            else
                                errs() << "Error: 2+ dimension array accessed - not compatible yet: " << *I << "\n";
                        }
                        else
                            *fout << "\t" << printRegNameInBoogie(I) << " := "<< printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                        
                        break;
                        
                    case 28:    // store
                        if (isa<GetElementPtrInst>((Instruction *)I->getOperand(1)))
                        {
                            for (auto ait = arrayInfo.begin(); ait != arrayInfo.end(); ++ait)
                            {   // get dimension
                                arrayNode *AN = *ait;
                                std::vector<uint64_t> size = AN->size;
                                GlobalVariable *addr = AN->addr;
                                
                                if (((Instruction *)I->getOperand(1))->getOperand(0) == addr)
                                {
                                    dim = size.size();
                                    break;
                                }
                            }
                            
                            if (dim == 0)
                                errs() << "Error: unknown array stored - cannot find size: " << *I << "\n";
                            else if (dim == 1)
                                *fout << "\t" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(2)) << "] := " << printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                            else if (dim == 2)
                                *fout << "\t" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(1)) << "][" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(2)) << "] := " << printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                            else
                                errs() << "Error: 2+ dimension array accessed - not compatible yet: " << *I << "\n";
                        }
                        else
                            *fout << "\t" << printRegNameInBoogie((Value *)I->getOperand(1)) << " := "<< printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                        break;
                    case 29:     // getelementptr                               // this can be ignored
                        /*for (auto ait = arrayInfo.begin(); ait != arrayInfo.end(); ++ait)
                        {   // get dimension
                            arrayNode *AN = *ait;
                            std::vector<uint64_t> size = AN->size;
                            GlobalVariable *addr = AN->addr;
                            
                            if (I->getOperand(0) == addr)
                            {
                                dim = size.size();
                                break;
                            }
                        }
                        
                        if (dim == 0)
                            errs() << "Error: unknown array accessed - cannot find size: " << *I << "\n";
                        else if (dim == 1)
                            *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)I->getOperand(0)) << "[" << printRegNameInBoogie((Value *)I->getOperand(2)) << "];\n";
                        else if (dim == 2)
                            *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)I->getOperand(0)) << "[" << printRegNameInBoogie((Value *)I->getOperand(1)) << "][" << printRegNameInBoogie((Value *)I->getOperand(2)) << "];\n";
                        else
                            errs() << "Error: 2+ dimension array accessed - not compatible yet: " << *I << "\n";*/
                            *fout << "\t" << printRegNameInBoogie(I) << " := 0bv32;\n";
                        break;
                        
                    case 33:     // trunc
                        // ignore trunc instructions
                        *fout << "\t" << printRegNameInBoogie(I) << " := "<< printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                        break;
                        
                    case 34:     // zext   - may be modified later
                        // possible types are i1, i8. i16, i24, i32, i40, i48, i56, i64, i88, i96, i128, ref, float
                        if (I->getType()->getTypeID() == 10) // check if it is integer type
                        {
                            IntegerType *resType = dyn_cast<IntegerType>(I->getType());
                            IntegerType *oprType = dyn_cast<IntegerType>(I->getOperand(0)->getType());
                            
                            if (!resType)
                                errs() << "Error found in getting result type of zext instruction: " << *I << "\n";
                            if (!oprType)
                                errs() << "Error found in getting operand type of zext instruction: " << *I << "\n";
                            
                            if (resType->getBitWidth() > oprType->getBitWidth())
                                *fout << "\t" << printRegNameInBoogie(I) << " := " << printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                            else
                            {
                                // do mod
                                // case errs() << printRegNameInBoogie(I) << ":=" << printRegNameInBoogie((Value *)I->getOperand(0)) << " % " << ??? << ";\n";
                            }
                        }
                        else
                            errs() << "Error: Undefined type in zext instruction: " << *I << "\n";
                        break;
                        
                    case 46:    // icmp
                        if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*I)) {
                            if (cmpInst->getPredicate() == CmpInst::ICMP_EQ) {
                                *fout << "\tif (" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_NE) {
                                *fout << "\tif (" << printRegNameInBoogie((Value *)I->getOperand(0)) << " != " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_UGT) {
                                *fout << "\tif (bv32ugt(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_UGE) {
                                *fout << "\tif (bv32uge(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_ULT) {
                                *fout << "\tif (bv32ult(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_ULE) {
                                *fout << "\tif (bv32ule(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SGT) {
                                *fout << "\tif (bv32sgt(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SGE) {
                                *fout << "\tif (bv32sge(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SLT) {
                                *fout << "\tif (bv32slt(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SLE) {
                                *fout << "\tif (bv32sle(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else
                                errs() << "Error: Instruction decoding error at icmp instruction: " << *I << "\n";
                        }
                        break;
                    case 48:     // phi
                        // Has been done in previous section
                        break;
                    case 49:     // call
                        errs() << "Error: Call functions found in thread functions: " << *I << "\n";
                        break;
                    case 50:     // select
                        *fout << "\tif (" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == 1bv32) { " << printRegNameInBoogie(I) << " := " << printRegNameInBoogie((Value *)I->getOperand(1)) <<  "; } else { " << printRegNameInBoogie(I) << " := " << printRegNameInBoogie((Value *)I->getOperand(2)) << "; }\n";
                        break;
                    default:
                        errs() << "Error: <Invalid operator>" << I->getOpcodeName() << "\t" << I->getOpcode() << "\n ";
                        I->print(errs());
                        errs() << "\n ";
                }   // end of switch
                
                for (auto bankAddrCheck = bankAddressInstrLsit.begin(); bankAddrCheck != bankAddressInstrLsit.end(); ++bankAddrCheck)
                {
                    if (*bankAddrCheck == I)
                    {
                        *fout << "\tif(*){\n\t\tindex_" << indexCounter << " := " << printRegNameInBoogie(I) << ";\n\t\tread_" << indexCounter << " := true;\n\t}\n\treturn;\n\tassume false;\n";
                        indexCounter++;
                    }
                }
            }                                                               // End of instruction level analysis
            
        }                                     // End of basic block level analysis
    }   // end of instrDecoding
    
    // This function is to print variable declaration in Boogie
    void EASY::varDeclaration(Value *instrVar, std::fstream *fout, bool isbool){
        *fout << "\tvar " << printRegNameInBoogie(instrVar) << ":bv32;\n";
    }   // end of varDeclaration
    
    // This function is to check if the variable has been declared
    bool EASY::varFoundInList(Value *instrVar, Function *F){
        bool funcFound = false;
        bool varFound = false;
        varList *list;
        
        // check if it is a constant
        if (dyn_cast<llvm::ConstantInt>(instrVar))
            return true;
        
        // search function in variable list
        for (auto it = codeVarList.begin(); it != codeVarList.end(); ++it)
        {
            varList *temp = *it;
            // errs() << "debug check function :" << F->getName() << "\n";// << " VS "<< temp->func->getName() << "\n";
            if (temp->func == F)
            {
                funcFound = true;
                // errs() << "Function " << F->getName() << " found.\n";
                list = *it;
                // Function found => search for var
                if (!isFuncArg(instrVar, F))
                {
                    for (auto itVar = list->vars.begin(); itVar != list->vars.end(); ++itVar)
                    {
                        Value *itvar = *itVar;
                        // errs() << "debug check var: " << printRegNameInBoogie(itvar) << "\n";
                        if (itvar == instrVar)
                            varFound = true;
                    }
                    if (!varFound)
                        list->vars.push_back(instrVar);
                }
                else
                    varFound = true;
            }
        }
        
        if (!funcFound)
        {
            // Function not found => start new
            list = new varList;
            if (!isFuncArg(instrVar, F))
            {
                list->func = F;
                list->vars.push_back(instrVar);
            }
            // errs() << "Function " << F->getName() << " not found and added.\n";
            codeVarList.push_back(list);
        }
        
        return varFound;
    }   // end of varFoundInList
    
    // This is function is determine if it is a function argument
    bool EASY::isFuncArg(Value *instrVar, Function *F){
        for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
        {
            Value *arg = fi;
            if (instrVar == arg)
                return true;
        }
        return false;
    }   // end of isFuncArg
    
    // This is a function to print hidden basic block label
    std::string EASY::getBlockLabel(BasicBlock *BB){
        std::string block_address;
        raw_string_ostream string_stream(block_address);
        BB->printAsOperand(string_stream, false);
        
        std::string temp = string_stream.str();
        
        for (unsigned int i = 0; i<temp.length(); ++i)
        {
            if (temp[i] == '-')
                temp.replace(i, 1, "_");
        }
        
        return temp;
    }   // end of getBlockLabel
    
    // This is a function to print IR variable name
    std::string EASY::printRegNameInBoogie(Value *I){
        std::string instrResName;
        raw_string_ostream string_stream(instrResName);
        I->printAsOperand(string_stream, false);
        if (ConstantInt *constVar = dyn_cast<ConstantInt>(I))
        {
            
            // errs() << "This is a constant: " << *I << "----";
            // if (constVar->getType()->isIntegerTy())
            if (constVar->isNegative())
            {
                string_stream.str().replace(0,1," ");
                // errs() << "Output test: bv32sub(0bv32, " + string_stream.str()+"bv32)\n";
                return ("bv32neg(" + string_stream.str()+"bv32)"); //*(constVar->getType())
            }
            else
                return(string_stream.str()+"bv32");
        }
        return string_stream.str();
    }       // end of printRegNameInBoogie
    
    // Analyze global memory accesses in thread function and slice
    void EASY::sliceThreadFunction(Module &M) {
        
        errs() << "\n*********************************************\n";
        errs() << "2. Extracting Global Memory Access Traces...\n";
        errs() << "*********************************************\n";
        for (auto F = M.begin(); F != M.end(); ++F) {                               // Function level
            if ((Function *)F == threadFunc)    // only slice gloabl memory access in thread function
            {
                Slice *threadFuncSlice = new Slice((Function*)F);
                for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
                    for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                        if (I->getOpcode() == 50) {     // select instruction
                            auto backIcmp = I;
                            auto backIcmpBB = BB;
                            while (backIcmp != F->begin()->begin())
                            {
                                if (backIcmp->getOpcode() == 46 && dyn_cast<Instruction>(I->getOperand(0)) == backIcmp)
                                {
                                    auto backBankAddr = backIcmp;
                                    auto backBankAddrBB = backIcmpBB;
                                    while (backBankAddr != F->begin()->begin())
                                    {
                                        if (dyn_cast<Instruction>(backIcmp->getOperand(0)) == backBankAddr)
                                        {
                                            if (backBankAddr->getOpcode() == 22 || backBankAddr->getOpcode() == 23)
                                            {
                                                if (bankAddressInstrLsit.size() == 0)
                                                bankAddressInstrLsit.push_back(backBankAddr);
                                                else
                                                {
                                                    int i = 0;
                                                    for (auto bankAddrCheck = bankAddressInstrLsit.begin(); bankAddrCheck != bankAddressInstrLsit.end(); ++bankAddrCheck)
                                                    {
                                                        if (*bankAddrCheck == backBankAddr)
                                                        i = 1;
                                                    }
                                                    if (i == 0)
                                                    bankAddressInstrLsit.push_back(backBankAddr);
                                                }
                                            }
                                        }
                                        if (backBankAddr == backBankAddrBB->begin())
                                        {
                                            backBankAddrBB--;
                                            backBankAddr = backBankAddrBB->end();
                                            backBankAddr--;
                                        }
                                        else
                                        backBankAddr--;
                                    }
                                }
                                if (backIcmp == backIcmpBB->begin())
                                {
                                    backIcmpBB--;
                                    backIcmp = backIcmpBB->end();
                                    backIcmp--;
                                }
                                else
                                backIcmp--;
                            }
                            threadFuncSlice->addCriterion(I);   // added into critia
                        }   // end if == 50
                    }              // End of instruction slevel
                }                  // End of basic block level
                // debug purpose
                // threadFuncSlice->print();
                ISlice *inversedThreadFuncSlice = new ISlice((Function*)F);
                inversedThreadFuncSlice->inverse(threadFuncSlice);
                removeSlice((Function *)F, inversedThreadFuncSlice);
                errs() << "=> Removed " << inversedThreadFuncSlice->instructionCount() << " instructions from function (" << F->getName() << ").\n";
            }
        }
    }   // end of sliceThreadFunction
    
    // This function is to extract the information of function threads
    void EASY::analyzeThreadInfo(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "0. Analyzing thread function...\n";
        errs() << "*********************************************\n\n";
        
        std::vector<Instruction *> call;
        
        // Thread function search
        for (auto F = M.begin(); F != M.end(); ++F) {                               // Function level
            for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
                for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                    if (isa<GetElementPtrInst>(I)) {
                        // getelementptrs - This is related to global array
                        if (F->getName() != "main")
                            // main function would not be considered as no memory contention in single thread...
                            threadFunc = F;
                    } else if (isa<CallInst>(I)) {
                        call.push_back(I);  // collect for thread counting
                    }
                }                                                               // End of instruction level analysis
            }                                                                   // End of basic block level analysis
        }                                                                       // End of function level analysis
        
        // Thread number counting
        for (auto call_it = call.begin(); call_it != call.end(); ++call_it)
        {
            Instruction *call_temp = *call_it;
            if (call_temp->getOperand(call_temp->getNumOperands()-1) == threadFunc)
                // check if it is calling the thread fucntion
                threadNum++;
        }
        
        errs() << "Thread function name: " << threadFunc->getName() << "\n";
        errs() << "Thread number: " << threadNum << "\n";
    }       // end of analyzeThreadInfo

    // Print Boogie code header
    void EASY::printBoogieHeader(void) {
        bpl << "\n//*********************************************\n";
        bpl <<   "//    Boogie code generated from LLVM\n";
        bpl <<   "//*********************************************\n";

        bpl << "// Bit vector function prototypes\n";
        bpl << "// Arithmetic\n";
        bpl << "function {:bvbuiltin \"bvadd\"} bv32add(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsub\"} bv32sub(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvmul\"} bv32mul(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvudiv\"} bv32udiv(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvurem\"} bv32urem(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsdiv\"} bv32sdiv(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsrem\"} bv32srem(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsmod\"} bv32smod(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvneg\"} bv32neg(bv32) returns(bv32);\n";
        bpl << "// Bitwise operations\n";
        bpl << "function {:bvbuiltin \"bvand\"} bv32and(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvor\"} bv32or(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvnot\"} bv32not(bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvxor\"} bv32xor(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvnand\"} bv32nand(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvnor\"} bv32nor(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvxnor\"} bv32xnor(bv32,bv32) returns(bv32);\n";
        bpl << "// Bit shifting\n";
        bpl << "function {:bvbuiltin \"bvshl\"} bv32shl(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvlshr\"} bv32lshr(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvashr\"} bv32ashr(bv32,bv32) returns(bv32);\n";
        bpl << "// Unsigned comparison\n";
        bpl << "function {:bvbuiltin \"bvult\"} bv32ult(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvule\"} bv32ule(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvugt\"} bv32ugt(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvuge\"} bv32uge(bv32,bv32) returns(bool);\n";
        bpl << "// Signed comparison\n";
        bpl << "function {:bvbuiltin \"bvslt\"} bv32slt(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvsle\"} bv32sle(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvsgt\"} bv32sgt(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvsge\"} bv32sge(bv32,bv32) returns(bool);\n\n";
    }
    
    // Collects and prints out statistics on partitioned arrays in the program - modified from Joy's code
    void EASY::findPartitionedArrays(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "1. Finding Partitioned Arrays...\n";
        errs() << "*********************************************\n\n";
        
        printBoogieHeader();
        bpl << "// global array in the program:\n";
        
        // Analyze all global variables - only ConstantArray and ConstantDataArray so far
        for (Module::global_iterator gi = M.global_begin(), ge = M.global_end();
             gi != ge; ++gi) {
            GlobalVariable *gVar = gi;
            Constant *c = gVar->getInitializer();
            std::vector<uint64_t> dims;
            dims.clear();
            int numElem = 0;
            Type *elemTy = NULL;

            if (gi->getName().str().at(0) == '.') {
                // errs() << "Not a user declared variable: " << gi->getName()) << "\n";
                continue;
            }
            
            if (isa<ConstantArray>(c)) {                            // analyze array type
                ArrayType *AT = dyn_cast<ConstantArray>(c)->getType();
                getGlobalArraySize(AT, dims, numElem, &elemTy);
                bpl << "var @" << gi->getName().str() << ": [bv32]bv32; // array_size = " << numElem << "\n";
            } else if (isa<ConstantDataArray>(c)) {
                ArrayType *AT = dyn_cast<ConstantDataArray>(c)->getType();
                getGlobalArraySize(AT, dims, numElem, &elemTy);
                bpl << "var @" << gi->getName().str() << ": [bv32]bv32; // array_size = " << numElem << "\n";
            } else if (isa<ConstantAggregateZero>(c)) {             // aggregate initialized to zero
                ArrayType *AT = dyn_cast<ArrayType>(dyn_cast<ConstantAggregateZero>(c)->getType());
                if (!AT)
                    continue; // not array type
                getGlobalArraySize(AT, dims, numElem, &elemTy);
                bpl << "var @" << gi->getName().str() << ": [bv32]bv32; // array_size = " << numElem << "\n";
            } else {
                errs() << "Neither ConstantArray nor ConstantDataArray: " << gi->getName()<< "\n";
                continue;
            }                                                       // in this case, all the array should be analyzed
            
            // create an array node
            arrayNode *AN = new arrayNode;
            AN->name = gi->getName();
            AN->addr = gVar;
            AN->size.clear();
            AN->size = dims;
            AN->elemTy = elemTy;
            
            arrayInfo.push_back(AN);
            // This arrayInfo stores the information of all the constant global arrays
        }
        
        std::string partitionedArrayName;
        int nameLength;
        for (auto it = arrayInfo.begin(), et = arrayInfo.end(); it != et; ++it) {
            arrayNode *AN = *it;
            std::string arrayName;
            arrayName = AN->name;
            
            // since currently all the arrays have been partitioned into the same number of mmeory banks
            // pick any partitioned array name can count the partition number
            if (arrayName.substr (0,4) == "sub_" && arrayName.substr (arrayName.length()-2,2) == "_0")
            {
                nameLength = arrayName.length()-6;
                partitionedArrayName = arrayName.substr(4, nameLength);
                errs() << "Partitioned array name: " << partitionedArrayName << "\n";
            }
        }
        
        for (auto it = arrayInfo.begin(), et = arrayInfo.end(); it != et; ++it) {
            arrayNode *AN = *it;
            std::string arrayName;
            arrayName = AN->name;
            
            // compare the original name of the array to identify if it has been partitioned
            // used to count the partition number
            if (partitionedArrayName == arrayName.substr(4, nameLength))
                partitionNum++;
        }
        errs() << "Partition number: " << partitionNum << "\n";
    }   // end of findPartitionedArrays
    
    // copied from Joy's pass
    bool EASY::getGlobalArraySize(ArrayType *array,
                                                                  std::vector<uint64_t> &dims,
                                                                  int &numElem, Type **elemTy) {
        int resultNumElem = 1;
        
        Type *elementType;
        ArrayType *AT = array;
        
        do {
            dims.push_back((uint64_t)AT->getNumElements());
            assert(AT->getNumElements() > 0);
            resultNumElem *= AT->getNumElements();
            elementType = AT->getElementType();
            AT = dyn_cast<ArrayType>(AT->getElementType());
        } while (AT && (AT->getTypeID() == Type::ArrayTyID));
        
        assert(elementType);
        assert(isa<Type>(elementType));
        
        // errs() << "TypeID: " << elementType->getTypeID() << "\n";
        // elementType->print(errs());
        // errs() << "\n";
        
        *elemTy = elementType;
        numElem = resultNumElem;
        
        return true;
    } // end of getGlobalArraySize
    
    // Translate IR code to Boogie
    void EASY::interpretToCSimulator(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "4.5. Converting LLVM IR Code to C Code...\n";
        errs() << "*********************************************\n";
        
        std::fstream csim;
        csim.open ("simulator.cpp", std::fstream::out);
        
        csim << "\n//*********************************************\n";
        csim <<   "//    C Simulator generated from LLVM\n";
        csim <<   "//*********************************************\n";
        
        csim << "#include <iostream>\n";
        csim << "#include <vector>\n";
        
        // get all pointer values (interprocedural)
        for (auto F = M.begin(); F != M.end(); ++F) {    // Function level
            if (F->size() != 0)                          // ignore all empty functions
            {
                // print function prototypes
                if (static_cast<std::string>((F->getName())) == "main")
                    // main function prototype
                    csim << "\nint main(void) \n{\n";
                else
                {
                    // other thread functions
                    csim << "\nint " << static_cast<std::string>((F->getName())) << "(";
                    if (!(F->arg_empty()))
                    {
                        for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
                        {
                            csim << "int " << printRegNameInC(fi);
                            
                            auto fi_comma = fi;
                            fi_comma++;
                            if (fi_comma != F->arg_end())
                                csim << ", ";
                        }
                    }
                    // only returns the bank address information
                    csim << ")\n{\n";
                }
                
                
                if (F->getName() != "main")
                {
                    
                    // stop here -----------------------------------------------------
                    // variable definitions
                    for (auto it = codeVarList.begin(); it != codeVarList.end(); ++it)
                    {
                        varList *temp = *it;
                        if (temp->func == F)
                        {
                            for (auto itInstr = (temp->vars).begin(); itInstr != (temp->vars).end(); ++itInstr)
                                csim << "int " << printRegNameInC((Value *)*itInstr) << ";\n";
                        }
                    }
                    csim << "int index = -1;\n";
                    csim << "\n";
                    // decode all instructions
                    instrDecodingC(F, &csim);
                }
                else
                {
                    // just print thread function information in main
                    for (int i = 0; i < threadNum; i++)
                        csim << "\tint t" << i << "_index;\n";
                    csim << "\n";
                    int t_i = 0;
                    // print call instructions
                    for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
                        for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                            if (I->getOpcode() == 49) {
                                if (I->getOperand(I->getNumOperands()-1) == threadFunc)
                                {
                                    csim << "\t// thread function call " << t_i << ": \n";
                                    csim << "\tstd::cout << \"For function: " << static_cast<std::string>((F->getName())) << ", thread: " << t_i << "\\n\";\n";
                                    csim << "\tt" << t_i << "_index = " << static_cast<std::string>((I->getOperand(I->getNumOperands()-1))->getName()) << "(";
                                    if (I->getNumOperands() > 1)
                                    {
                                        csim << printRegNameInC((Value *)I->getOperand(0));
                                        for (unsigned int i = 1; i != (I->getNumOperands()- 1); ++i)
                                            csim << "," << printRegNameInC((Value *)I->getOperand(i));
                                    }
                                    csim << ");\n";
                                    t_i++;
                                }   // end if
                            }   // end if
                        }      // End of instruction level
                    }// End of basic block level
                }
                
                csim << "}\n";   // indicate end of function
                // errs() << "check1\n";
            }
            else
                errs() << "Function: " << static_cast<std::string>((F->getName())) << "is empty so ignored in Boogie\n";
        }                                                                       // End of function level analysis
        // errs() << "check\n";
        csim.close();
        errs() << "\nTransfering to C finished. \n";
    }   // end of interpretToCSimulator
    
    // Translate all IR instruction to Boogie instruction
    void EASY::instrDecodingC(Function *F, std::fstream *fout)
    {
        // mark
        for (unsigned int j = 0; j < bankAddressInstrLsit.size(); j++)
            *fout << "\tstd::vector<int> bankAddrList_" << j << ";\n\tbool searchFlag_" << j << " = false;\n";
        int indexCounter = 0;
        for (auto BB = F->begin(); BB != F->end(); ++BB)
        {
            *fout << "\n\t// For basic block: bb_" << printRegNameInC((Value *)BB) << "\n";
            *fout << "\tbb_" << printRegNameInC((Value *)BB) << ":\n";
            // Start instruction printing
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                // start printing end loop assertion here
                
                switch (I->getOpcode()) {
                    case 1:     // ret
                        *fout << "\treturn index;\n";  //default return - memory not accessed.
                        break;
                        
                    case 2:     // br
                        // do phi resolution here
                        for (auto it = phiList.begin(); it != phiList.end(); ++it)
                        {
                            phiNode *phiTfInst = *it;
                            if (phiTfInst->bb == BB)
                                *fout << "\t" << printRegNameInC((Value *)phiTfInst->instr) << " = " << printRegNameInC(phiTfInst->ip) << ";\n";
                        }
                        // add branch instruction here
                        if (I->getNumOperands() == 1) // if (inst->isConditional())?
                            *fout << "\tgoto bb_"<< printRegNameInC((Value *)(BasicBlock *)(I->getOperand(0))) << ";\n";
                        else if (I->getNumOperands() == 3)
                            *fout << "\tif(" << printRegNameInC((Value *)I->getOperand(0)) << " == 1) {goto bb_"<< printRegNameInC((Value *)(BasicBlock *)(I->getOperand(2))) << ";} else {goto bb_" << printRegNameInC((Value *)(BasicBlock *)(I->getOperand(1))) << ";}\n";
                        else
                            errs() << "Error: Instruction decoding error at br instruction: " << *I << "\n";
                        break;
                        
                    case 8:     // add
                        if (OverflowingBinaryOperator *op = dyn_cast<OverflowingBinaryOperator>(I)) {
                            if ((op->hasNoUnsignedWrap()) && (op->hasNoSignedWrap()))
                                // has both nuw and nsw
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " + " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                            else if (op->hasNoUnsignedWrap())
                                // only nuw
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " + " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                            else if (op->hasNoSignedWrap())
                                // only nsw
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " + " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                            else
                                // normal add
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " + " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";
                        }
                        else
                            errs() << "Error: Instruction decoding error at add instruction: " << *I << "\n";
                        break;
                        
                    case 10:     // sub
                        if (OverflowingBinaryOperator *op = dyn_cast<OverflowingBinaryOperator>(I)) {
                            if ((op->hasNoUnsignedWrap()) && (op->hasNoSignedWrap()))
                                // has both nuw and nsw
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " - " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                            else if (op->hasNoUnsignedWrap())
                                // only nuw
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " - " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                            else if (op->hasNoSignedWrap())
                                // only nsw
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " - " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                            else
                                // normal add
                                *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " - " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";       // ---might has problems...
                        }
                        else
                            errs() << "Error: Instruction decoding error at sub instruction: " << *I << "\n";
                        break;
                        
                    case 12:     // mul
                        *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " * " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";
                        break;
                    case 20:     // shl
                        *fout << "\t" << printRegNameInC(I) << " = ("<< printRegNameInC((Value *)I->getOperand(0)) << " << " << printRegNameInC((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                        break;
                    case 21:     // lshr
                        *fout << "\t" << printRegNameInC(I) << " = ("<< printRegNameInC((Value *)I->getOperand(0)) << " >> " << printRegNameInC((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                        break;
                        
                    case 22:     // ashr
                        *fout << "\t" << printRegNameInC(I) << " = ("<< printRegNameInC((Value *)I->getOperand(0)) << " >> " << printRegNameInC((Value *)I->getOperand(1)) << ");\n";
                        break;
                    case 23:     // and
                        *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " & " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";
                        break;
                    case 24:     // or
                        *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << " | " << printRegNameInC((Value *)I->getOperand(1)) << ";\n";
                        break;
                    case 27:     // load
                        // ignore load instructions
                        *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << ";\n";
                        break;
                        
                    case 29:     // getelementptr                               // this is can be ignored
                        *fout << "\t" << printRegNameInC((Value *)I) << " = 0;\n";
                        break;
                        
                    case 33:     // trunc
                        // ignore trunc instructions
                        *fout << "\t" << printRegNameInC(I) << " = "<< printRegNameInC((Value *)I->getOperand(0)) << ";\n";
                        break;
                        
                    case 34:     // zext   - may be modified later
                        // possible types are i1, i8. i16, i24, i32, i40, i48, i56, i64, i88, i96, i128, ref, float
                        if (I->getType()->getTypeID() == 10) // check if it is integer type
                        {
                            IntegerType *resType = dyn_cast<IntegerType>(I->getType());
                            IntegerType *oprType = dyn_cast<IntegerType>(I->getOperand(0)->getType());
                            
                            if (!resType)
                                errs() << "Error found in getting result type of zext instruction: " << *I << "\n";
                            if (!oprType)
                                errs() << "Error found in getting operand type of zext instruction: " << *I << "\n";
                            
                            if (resType->getBitWidth() > oprType->getBitWidth())
                                *fout << "\t" << printRegNameInC(I) << " = " << printRegNameInC((Value *)I->getOperand(0)) << ";\n";
                            else
                            {
                                // do mod
                                // case errs() << printRegNameInC(I) << ":=" << printRegNameInC((Value *)I->getOperand(0)) << " % " << ??? << ";\n";
                            }
                        }
                        else
                            errs() << "Error: Undefined type in zext instruction: " << *I << "\n";
                        break;
                        
                    case 46:    // icmp
                        if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*I)) {
                            if (cmpInst->getPredicate() == CmpInst::ICMP_EQ) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " == " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_NE) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " != " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_UGT) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " > " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_UGE) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " >= " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_ULT) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " < " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_ULE) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " <= " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SGT) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " > " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SGE) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " >= " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SLT) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " < " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SLE) {
                                *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " <= " << printRegNameInC((Value *)I->getOperand(1)) << ") { " << printRegNameInC(I) << " = 1; } else { " << printRegNameInC(I) << " = 0; }\n"; // ---might has problems...
                            }
                            else
                                errs() << "Error: Instruction decoding error at icmp instruction: " << *I << "\n";
                        }
                        break;
                    case 48:     // phi
                        // Has been done in previous section
                        break;
                    case 49:     // call
                        errs() << "Error: Call functions found in thread functions: " << *I << "\n";
                        break;
                    case 50:     // select
                        *fout << "\tif (" << printRegNameInC((Value *)I->getOperand(0)) << " == 1) { " << printRegNameInC(I) << " = " << printRegNameInC((Value *)I->getOperand(1)) <<  "; } else { " << printRegNameInC(I) << " = " << printRegNameInC((Value *)I->getOperand(2)) << "; }\n";
                        break;
                    default:
                        errs() << "Error: <Invalid operator>" << I->getOpcodeName() << "\t" << I->getOpcode() << "\n ";
                        I->print(errs());
                        errs() << "\n ";
                }   // end of switch
                
                for (auto bankAddrCheck = bankAddressInstrLsit.begin(); bankAddrCheck != bankAddressInstrLsit.end(); ++bankAddrCheck)
                {
                    if (*bankAddrCheck == I)
                    {
                        *fout << "\tindex = " << printRegNameInC(I) << ";\n";
                        
                        *fout << "\tsearchFlag_" << indexCounter << " = false;\n";
                        *fout << "\tfor (auto it = bankAddrList_" << indexCounter << ".begin(); it != bankAddrList_" << indexCounter << ".end(); ++it)\n";
                        *fout << "\t{\n";
                        *fout << "\t\tif (index == *it)\n";
                        *fout << "\t\t\tsearchFlag_" << indexCounter << " = true;\n";
                        *fout << "\t}\n";
                        *fout << "\tif (!searchFlag_" << indexCounter << ")\n";
                        *fout << "\t{\n";
                        *fout << "\t\tstd::cout << \"Index_" << indexCounter << " = \" << index << \"\\n\";\n";
                        *fout << "\t\tbankAddrList_" << indexCounter << ".push_back(index);\n";
                        *fout << "\t}\n";
                        indexCounter++;
                    }
                }
            }                                                               // End of instruction level analysis
            
        }                                     // End of basic block level analysis
        
    }   // end of instrDecodingC
    
    std::string EASY::printRegNameInC(Value *I){
        std::string instrResName;
        raw_string_ostream string_stream(instrResName);
        I->printAsOperand(string_stream, false);
        /*if (ConstantInt *constVar = dyn_cast<ConstantInt>(I))
         {
         
         // errs() << "This is a constant: " << *I << "----";
         // if (constVar->getType()->isIntegerTy())
         if (constVar->isNegative())
         {
         string_stream.str().replace(0,1," ");
         // errs() << "Output test: bv32sub(0bv32, " + string_stream.str()+"bv32)\n";
         return ("bv32neg(" + string_stream.str()+"bv32)"); // *(constVar->getType())
         }
         else
         return(string_stream.str()+"bv32");
         }*/
        
        std::string temp = string_stream.str();
        
        for (unsigned int i = 0; i<temp.length(); ++i)
        {
            if (temp[i] == '.')
                temp.replace(i, 1, "_");
            if (temp[i] == '-' && i > 0)
                temp.replace(i, 1, "_");
        }
        
        return temp;
    }       // end of printRegNameInBoogie
    
    
    char EASY::ID = 0;
    static RegisterPass<EASY> X("EASY", "Global Memory Partitioning Verification Program",
                                                                false /* Only looks at CFG */,
                                                                false /* Analysis Pass */);
    
}  // end of legup namespace
