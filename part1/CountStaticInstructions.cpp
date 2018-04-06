#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"

using namespace llvm;
using namespace std;

namespace {
struct CountStaticInstructions : public FunctionPass {
  static char ID;
  CountStaticInstructions() : FunctionPass(ID) {}
  map<string, int> dict;

  bool runOnFunction(Function &F) override {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      string name = I->getOpcodeName();
      dict[name] += 1;     
    }
    for (map<string,int>::iterator it=dict.begin(); it!=dict.end(); ++it) {
      errs() << it->first << '\t' << it->second << '\n';
    }

    return false;
  }
}; // end of struct TestPass
}  // end of anonymous namespace

char CountStaticInstructions::ID = 0;
static RegisterPass<CountStaticInstructions> X("cse231-csi", "Developed to test LLVM and docker",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);