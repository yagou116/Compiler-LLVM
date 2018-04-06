#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"

#include "231DFA.h"
#include <set>
#include <string>


using namespace llvm;
using namespace std;

namespace {

  class ReachingInfo : public Info{
    public:

      ReachingInfo() {}

      set<unsigned> defInstrs;

      /* Print out the information */
      void print(){
        for(auto def : defInstrs)
          errs() << def << "|";
        errs() << "\n";
      }

      /* Compare two pieces of information */
      static bool equals(ReachingInfo * info1, ReachingInfo * info2){
        return info1->defInstrs == info2->defInstrs;
      }

      /* Join two pieces of information.
         The third parameter points to the result. */
      static ReachingInfo * join(ReachingInfo * info1, ReachingInfo * info2, ReachingInfo * result){
        if (!equals(result, info1))
          result->defInstrs.insert(info1->defInstrs.begin(), info1->defInstrs.end());
        if (!equals(result, info2))
          result->defInstrs.insert(info2->defInstrs.begin(), info2->defInstrs.end());
        return result;
      }

  };


  class ReachingDefinitionAnalysis : public DataFlowAnalysis<ReachingInfo, true> {

    public:
      ReachingDefinitionAnalysis(ReachingInfo & bottom, ReachingInfo & initialState) :
                     DataFlowAnalysis(bottom, initialState) {}


      void flowfunction(Instruction * I,
                        std::vector<unsigned> & IncomingEdges,
                        std::vector<unsigned> & OutgoingEdges,
                        std::vector<ReachingInfo *> & Infos) {

        unsigned instrIdx = InstrToIndex[I];

        // join the incoming data flows
        ReachingInfo * tempInfo = new ReachingInfo();
        for (auto incoming : IncomingEdges) {
          Edge e = std::make_pair(incoming, instrIdx);
          ReachingInfo::join(tempInfo, EdgeToInfo[e], tempInfo);
        }

        string opName = I->getOpcodeName();

        // category 1
        if (opName == "alloca" || opName == "load" || opName == "getelementptr" || 
            opName == "icmp" || opName == "fcmp" || opName == "select" ||
            I->isBinaryOp()) {

          tempInfo->defInstrs.insert(instrIdx);
        }

        // category 3
        if(opName == "phi"){     
          Instruction * firstNonPhi = I->getParent()->getFirstNonPHI();
          unsigned firstNonPhiIdx = InstrToIndex[firstNonPhi];
          for (unsigned i = instrIdx; i < firstNonPhiIdx; ++i)
            tempInfo->defInstrs.insert(i);
        }

        for(unsigned i = 0; i < OutgoingEdges.size(); ++i){
          Infos.push_back(tempInfo);
        }

      }
  };



  struct ReachingDefinitionAnalysisPass : public FunctionPass {
    static char ID;

    ReachingDefinitionAnalysisPass() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ReachingInfo bottom;
      ReachingInfo initialState;

      ReachingDefinitionAnalysis rda(bottom, initialState);
      rda.runWorklistAlgorithm(&F);
      rda.print();
      return false;
    }

  }; 

}  

char ReachingDefinitionAnalysisPass::ID = 0;
static RegisterPass<ReachingDefinitionAnalysisPass> X("cse231-reaching", "Reaching definition analysis", false, false);