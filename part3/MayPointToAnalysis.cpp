#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"

#include "231DFA.h"
#include <set>
#include <string>
#include <map>


using namespace llvm;
using namespace std;

// "R"/"M", index
typedef pair<char, unsigned> pointerInfo;

namespace {

  class MayPointToInfo : public Info {
    public:

      MayPointToInfo() {}

      map<pointerInfo, set<pointerInfo>> pointerMap;

       /* Print out the information */
      void print() {
        for(auto pointer : pointerMap) {
          errs() << pointer.first.first << pointer.first.second << "->(";
          for(auto pointee : pointer.second) {
            errs() << pointee.first << pointee.second << "/";
          }
          errs() << ")|";
        }
        errs() << "\n";
      }

      /* Compare two pieces of information */
      static bool equals(MayPointToInfo *info1, MayPointToInfo *info2) {
        auto map1 = info1->pointerMap;
        auto map2 = info2->pointerMap;
        return map1.size() == map2.size() && std::equal(map1.begin(), map1.end(), map2.begin());
      }

      /* Join two pieces of information.
         The third parameter points to the result. */
      static MayPointToInfo* join(MayPointToInfo *info1, MayPointToInfo *info2, MayPointToInfo *result) {
        result->pointerMap.insert(info1->pointerMap.begin(), info1->pointerMap.end());
        result->pointerMap.insert(info2->pointerMap.begin(), info2->pointerMap.end());
        return result;
      }   

    };


  class MayPointToAnalysis : public DataFlowAnalysis<MayPointToInfo, true> {

    public:

      MayPointToAnalysis(MayPointToInfo & bottom, MayPointToInfo & initState) : 
                    DataFlowAnalysis<MayPointToInfo, true>(bottom, initState) {}

      void flowfunction(Instruction * I,
                        std::vector<unsigned> & IncomingEdges,
                        std::vector<unsigned> & OutgoingEdges,
                        std::vector<MayPointToInfo *> & Infos) {

        unsigned instrIdx = InstrToIndex[I];

        // join the incoming data flows
        MayPointToInfo *tempInfo = new MayPointToInfo();       

        for(auto incoming : IncomingEdges) {
          Edge e = std::make_pair(incoming, instrIdx);
          MayPointToInfo::join(EdgeToInfo[e], tempInfo, tempInfo);
        }

        string opName = I->getOpcodeName();


        // alloca: in U {Ri->Mi}
        if (opName == "alloca") {
          tempInfo->pointerMap[make_pair('R', instrIdx)].insert(make_pair('M', instrIdx));
        }    

        // bitcast / getelementptr: in U {Ri->X | Rv->X\in in}
        else if (opName == "bitcast" || opName == "getelementptr"){
          Instruction * operand = (Instruction *)I->getOperand(0);
          unsigned operandIdx = InstrToIndex[operand];
          pointerInfo Rv = make_pair('R', operandIdx);
          
          if(tempInfo->pointerMap.find(Rv) != tempInfo->pointerMap.end()) {
            for(auto x : tempInfo->pointerMap[Rv]) {
              tempInfo->pointerMap[make_pair('R',instrIdx)].insert(x);
            }
          }
        }

        // load: in U {Ri->Y | Rp->X\in in & X->Y\in in}
        else if (opName == "load"){

          Instruction * operand = (Instruction *)I->getOperand(0);
          unsigned operandIdx = InstrToIndex[operand];
          pointerInfo Rp = make_pair('R', operandIdx);

          if(tempInfo->pointerMap.find(Rp) != tempInfo->pointerMap.end()) {
            for(auto x : tempInfo->pointerMap[Rp]) {
              if(tempInfo->pointerMap.find(x) != tempInfo->pointerMap.end()) {
                for(auto y : tempInfo->pointerMap[x]) {
                  tempInfo->pointerMap[make_pair('R',instrIdx)].insert(y);
                }
              }
            }
          }
        }

        // store: in U {Y->X | Rv->X\in in & Rp->Y\in in}
        else if (opName == "store"){

          Instruction * operand0 = (Instruction *)I->getOperand(0);
          unsigned operandIdx0 = InstrToIndex[operand0];
          pointerInfo Rv = make_pair('R', operandIdx0);

          Instruction * operand1 = (Instruction *)I->getOperand(1);
          unsigned operandIdx1 = InstrToIndex[operand1];
          pointerInfo Rp = make_pair('R', operandIdx1);

          if(tempInfo->pointerMap.find(Rv) != tempInfo->pointerMap.end()) {
            for(auto x : tempInfo->pointerMap[Rv]) {
              if(tempInfo->pointerMap.find(Rp) != tempInfo->pointerMap.end()) {
                for(auto y : tempInfo->pointerMap[Rp]) {
                  tempInfo->pointerMap[y].insert(x);
                }
              }
            }
          }
        }

        // select: in U {Ri->X | R1->X\in in} U {Ri->X | R2->X\in in}
        else if (opName == "select"){

          Instruction * operand1 = (Instruction *)I->getOperand(1);
          unsigned operandIdx1 = InstrToIndex[operand1];
          pointerInfo R1 = make_pair('R', operandIdx1);

          Instruction * operand2 = (Instruction *)I->getOperand(2);
          unsigned operandIdx2 = InstrToIndex[operand2];
          pointerInfo R2 = make_pair('R', operandIdx2);

          if(tempInfo->pointerMap.find(R1) != tempInfo->pointerMap.end()) {
            for(auto x : tempInfo->pointerMap[R1]) {
              tempInfo->pointerMap[make_pair('R',instrIdx)].insert(x);
            }
          }

          if(tempInfo->pointerMap.find(R2) != tempInfo->pointerMap.end()) {
            for(auto x : tempInfo->pointerMap[R2]) {
              tempInfo->pointerMap[make_pair('R',instrIdx)].insert(x);
            }
          }
        }

        // phi: in U {Ri->X | R0->X\in in} U ... U {Ri->X | Rk->X\in in}
        else if (opName == "phi") {

          Instruction * firstNonPhi = I->getParent()->getFirstNonPHI();
          unsigned firstNonPhiIdx = InstrToIndex[firstNonPhi];

          for (unsigned i = instrIdx; i < firstNonPhiIdx; ++i) {
            Instruction * instr = IndexToInstr[i];
            PHINode * phiInstr = (PHINode *) instr;

            for (unsigned j = 0; j < phiInstr->getNumIncomingValues(); ++j) {
              Instruction * operand = (Instruction *)I->getOperand(j);
              unsigned operandIdx = InstrToIndex[operand];
              pointerInfo Rj = make_pair('R',operandIdx);

              if (tempInfo->pointerMap.find(Rj) != tempInfo->pointerMap.end()) {
                for (auto x : tempInfo->pointerMap[Rj]) {
                  tempInfo->pointerMap[make_pair('R',instrIdx)].insert(x);
                }
              }
            }
          }
        }

        for(unsigned i = 0; i < OutgoingEdges.size(); i++) {
          Infos.push_back(tempInfo);
        }

      } 


};

   struct MayPointToAnalysisPass : public FunctionPass {
      static char ID;

      MayPointToAnalysisPass() : FunctionPass(ID) {}

      bool runOnFunction(Function &F) override { 
        MayPointToInfo bottom;

        MayPointToAnalysis mpt(bottom, bottom);
        mpt.runWorklistAlgorithm(&F);
        mpt.print();
        return false;
      }
   }; 
}

char MayPointToAnalysisPass::ID = 0;
static RegisterPass<MayPointToAnalysisPass> X("cse231-maypointto", "May-Point-To Analysis", false, false);