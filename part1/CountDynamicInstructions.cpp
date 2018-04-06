#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"

using namespace llvm;
using namespace std;

namespace {
struct CountDynamicInstructions : public FunctionPass {
  static char ID;
  CountDynamicInstructions() : FunctionPass(ID) {}
  
  bool runOnFunction(Function &F) override {
    Module *M = F.getParent();

    /* define functions */
    Function * update = cast<Function>(M->getOrInsertFunction("updateInstrInfo_test", 
                                                                Type::getVoidTy(M->getContext()), 
                                                                Type::getInt32Ty(M->getContext()), 
                                                                Type::getInt32Ty(M->getContext())));
    Function * print = cast<Function>(M->getOrInsertFunction("printOutInstrInfo", Type::getVoidTy(M->getContext())));

    for (Function::iterator B = F.begin(); B != F.end(); ++B){
      map<int, int> dict;
      int flag = 0; // for "return" checking in block B
      for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I){
        int code = I->getOpcode();
        dict[code] += 1;

        if (code == 1){ // "return"
          flag = 1;
          /* update - before instruction I*/
          IRBuilder<> builder(&*I); 
          /* call one k-v pair one time */
          for (map<int,int>::iterator it=dict.begin(); it!=dict.end(); ++it){
            int key = it->first;
            int value = it->second;
            vector<Value *> args;
            args = {builder.getInt32(key), builder.getInt32(value)};
            builder.CreateCall(update, args);
          }
          /* print */
          builder.CreateCall(print);

        }

      }
      if (flag == 0){
        /* update - last instruction in B*/
        IRBuilder<> builder(&*(--B->end())); // insert after block B; call one k-v pair one time
        for (map<int,int>::iterator it=dict.begin(); it!=dict.end(); ++it){
          int key = it->first;
          int value = it->second;
          vector<Value *> args;
          args = {builder.getInt32(key), builder.getInt32(value)};
          builder.CreateCall(update, args);
        }
      }     
    }

    return true;
  }
}; // end of struct CountDynamicInstructions
}  // end of anonymous namespace

char CountDynamicInstructions::ID = 0;
static RegisterPass<CountDynamicInstructions> X("cse231-cdi", "Developed to test LLVM and docker",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);