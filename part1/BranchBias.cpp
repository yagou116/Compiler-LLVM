#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"



using namespace llvm;
using namespace std;

namespace {
struct BranchBias : public FunctionPass {
  static char ID;
  BranchBias() : FunctionPass(ID) {}
  
  bool runOnFunction(Function &F) override {
    Module *M = F.getParent();
    /* define functions */
    Function * update = cast<Function>(M->getOrInsertFunction("updateBranchInfo", 
                                                                Type::getVoidTy(M->getContext()), 
                                                                Type::getInt1Ty(M->getContext()))); // bool
    Function * print = cast<Function>(M->getOrInsertFunction("printOutBranchInfo", Type::getVoidTy(M->getContext())));

    for (Function::iterator B = F.begin(); B != F.end(); ++B){
      for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I){
        int code = I->getOpcode();

        if (code == 1){ // "return"
          /* print - before instruction I*/
          IRBuilder<> builder(&*I);
          builder.CreateCall(print);          
        }

        else if (code == 2 && I->getNumOperands() > 1) { // "br"
          /* update - before instruction I*/
          IRBuilder<> builder(&*I);
          vector<Value *> args;
          args = {I->getOperand(0)};
//          args = {builder.getInt1(I->getOperand(0))};
          builder.CreateCall(update, args);
        }
      }   
    }

    return true;
  }
}; // end of struct BranchBias
}  // end of anonymous namespace

char BranchBias::ID = 0;
static RegisterPass<BranchBias> X("cse231-bb", "Developed to test LLVM and docker",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
