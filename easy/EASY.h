//#ifndef LEGUP_EASY_H
//#define LEGUP_EASY_H
//#define PREDICATE 1

#include <set>
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "../../Target/Verilog/PADriver.h"
#include "../../IR/ConstantsContext.h"
//#include "../../Target/Verilog/PointerAnalysis.h"
//#include "RangeAnalysis.h"
//#include "utils.h"

#include "Slice.h"

using namespace llvm;

namespace EASY {
    
    class EASY : public ModulePass {
    public:
        static char ID; // Pass ID
        EASY() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) override;
        
    private:
        void initialization();
        void finalization();
        
        
        
        enum configType {NONE, ALL, BLOCK, CYCLIC, BLOCKCYCLIC};
        struct arrayNode {
            std::string name;
            std::vector<uint64_t> size;
            GlobalVariable *addr;
            Type *elemTy; // array element type
        };
        struct instructionNode {
            Function *func;
            std::string name;
            std::vector<Instruction *> instr;
        };
        struct phiNode {
            BasicBlock *bb;
            std::string res;
            Value *ip;
            Instruction* instr;
        };
        struct varList {
            Function *func;
            std::vector<Value *> vars;
        };
        struct invariance {
            Loop *loop;
            Instruction *indVarChange;
            std::string invar;
        };

        Module *Mod;
        std::fstream bpl;
        Function *threadFunc;
        int threadNum = 0;
        int partitionNum = 0;
        bool partition_flag;
        std::vector<arrayNode *> arrayInfo;
        std::vector<Slice *> slice_array;
        std::vector<instructionNode *> globalArrayAccessInstr;
        std::vector<instructionNode *> functionCallInstr;
        std::vector<varList *> codeVarList;
        std::vector<phiNode *> phiList;
        std::vector<invariance *> invarList;
        std::vector<Instruction *> bankAddressInstrLsit;
        
        void printBoogieHeader(void);
        void analyzeThreadInfo(Module &M);
        void findPartitionedArrays(Module &M);
        bool getGlobalArraySize(ArrayType *array,
                                std::vector<uint64_t> &dims,
                                int &numElem, Type **elemTy);
        void sliceThreadFunction(Module &M);
        void interpretToBoogie(Module &M);
        void printFunctionPrototype(Function *F, std::fstream *fout);
        void anlyzePhiInstr(Module &M);
        void printVarDeclarations(Function *F, std::fstream *fout);
        void printVarDeclarationsInMain(Function *F, std::fstream *fout);
        void instrDecoding(Function *F, std::fstream *fout);
        void instrDecodingInMain(Function *F, std::fstream *fout);
        bool varFoundInList(Value *instrVar, Function *F);
        bool isFuncArg(Value *instrVar, Function *F);
        void varDeclaration(Value *instrVar, std::fstream *fout, bool random);
        std::string getBlockLabel(BasicBlock *BB);
        std::string printRegNameInBoogie(Value *I);
        void instrDecodingC(Function *F, std::fstream *fout);
        void interpretToCSimulator(Module &M);
        std::string printRegNameInC(Value *I);
        
        
        
        // Might be global
        
        
        /*
         // Joy's code
         AliasAnalysis *AA;
         int instrNum;
         
         std::vector<arrayConfig> arrayPartitionConfig;
         
         void analyzeMemoryInstructions(Module &M);
         void findGlobalArrays(Module &M);
         
         void arrayAllocationStatistics(Module &M);
         void arrayAccessSummary();
         bool arraysFound();
         void readArrayConfigFromFile();
         void printConfigHelp(); */
        
        
        //bool doFinalization(Module &M) override {
        //  return false;
        //}
        
    }; // end of class EASY
    
} // namescape legup

