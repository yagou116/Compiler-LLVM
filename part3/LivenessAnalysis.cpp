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

   class LivenessInfo : public Info{
      public:

         LivenessInfo() {}

         set<unsigned> lives;

         /* Print out the information */
         void print(){
           for(auto def : lives)
             errs() << def << "|";
           errs() << "\n";
         }

         /* Compare two pieces of information */
         static bool equals(LivenessInfo * info1, LivenessInfo * info2){
           return info1->lives == info2->lives;
         }

         /* Join two pieces of information.
            The third parameter points to the result. */
         static LivenessInfo * join(LivenessInfo * info1, LivenessInfo * info2, LivenessInfo * result){
           result->lives.insert(info1->lives.begin(), info1->lives.end());
           result->lives.insert(info2->lives.begin(), info2->lives.end());
           return result;
         }
   };


   class LivenessAnalysis : public DataFlowAnalysis<LivenessInfo, false> {

      public:
         LivenessAnalysis(LivenessInfo & bottom, LivenessInfo & initialState) :
                     DataFlowAnalysis<LivenessInfo, false>(bottom, initialState) {}

         void flowfunction(Instruction * I,
                           std::vector<unsigned> & IncomingEdges,
                           std::vector<unsigned> & OutgoingEdges,
                           std::vector<LivenessInfo *> & Infos) {

            Infos.resize(OutgoingEdges.size());

            unsigned instrIdx = InstrToIndex[I];

            // join the incoming data flows
            LivenessInfo * tempInfo = new LivenessInfo();
    
            for (auto incoming : IncomingEdges) {
               Edge e = std::make_pair(incoming, instrIdx);
               LivenessInfo::join(EdgeToInfo[e], tempInfo, tempInfo);
            }

            string opName = I->getOpcodeName();

            // category 1： in[1] U ... U in[k] U operands - {index}
            if (opName == "alloca" || opName == "load" || opName == "getelementptr" || 
                opName == "icmp" || opName == "fcmp" || opName == "select" ||
                I->isBinaryOp()) {

               for (unsigned i = 0; i < I->getNumOperands(); ++i) {
                  Instruction * operand = (Instruction *)I->getOperand(i);
                  if (InstrToIndex.find(operand) != InstrToIndex.end()) {
                     tempInfo->lives.insert(InstrToIndex[operand]);
                  }
               } 

               tempInfo->lives.erase(instrIdx);

               for (unsigned k = 0; k < OutgoingEdges.size(); ++k) {
                  LivenessInfo* newInfo = new LivenessInfo();
                  newInfo->lives = tempInfo->lives;
                  Infos[k] = newInfo;
               }
            }

            // category 3: out[k] = in[1] U ... U - {result_i|i} U {ValuetoInstr(v_ij)|label k == label_ij}
            else if(opName == "phi"){ 

               Instruction * firstNonPhi = I->getParent()->getFirstNonPHI();
               unsigned firstNonPhiIdx = InstrToIndex[firstNonPhi];

               // - {result_i|i}
               for (unsigned i = instrIdx; i < firstNonPhiIdx; ++i) {
                  tempInfo->lives.erase(i);
               }

               for(unsigned j = 0; j < Infos.size() ; ++j){
                  Infos[j] = new LivenessInfo();
                  Infos[j]->lives = tempInfo->lives;
               }

               // U {ValuetoInstr(v_ij)|label k == label_ij}
               for (unsigned i =instrIdx; i < firstNonPhiIdx ; ++i) {
                  Instruction * instr = IndexToInstr[i];

                  PHINode * phiInstr = (PHINode *) instr;

                  for(unsigned j = 0; j < phiInstr->getNumIncomingValues(); ++j){

                     Instruction * value = (Instruction *)(phiInstr->getIncomingValue(j));

                     if(InstrToIndex.find(value) != InstrToIndex.end()){

                        BasicBlock * label = phiInstr->getIncomingBlock(j);
                        Instruction * labelInstr = (Instruction *)label->getTerminator();
                        unsigned labelInstrIdx = InstrToIndex[labelInstr];

                        for(unsigned k = 0; k < OutgoingEdges.size(); ++k){

                           if(OutgoingEdges[k] == labelInstrIdx){
                              Infos[k]->lives.insert(InstrToIndex[value]);
                           }
                        }
                     }
                  }   
               }
            }

            // category 2: in[1] U ... U in[k] U operands
            else{
               for (unsigned i = 0; i < I->getNumOperands(); ++i) {
                  Instruction * operand = (Instruction *)I->getOperand(i);
                  if (InstrToIndex.find(operand) != InstrToIndex.end()) {
                     tempInfo->lives.insert(InstrToIndex[operand]);
                  }
               } 

               for (unsigned k = 0; k < OutgoingEdges.size(); ++k) {
                  LivenessInfo* newInfo = new LivenessInfo();
                  newInfo->lives = tempInfo->lives;
                  Infos[k] = newInfo;
               }
            }

            delete tempInfo;
         }
      };




   struct LivenessAnalysisPass : public FunctionPass {
       static char ID;

       LivenessAnalysisPass() : FunctionPass(ID) {}

       bool runOnFunction(Function &F) override {
         LivenessInfo bottom;

         LivenessAnalysis la(bottom, bottom);
         la.runWorklistAlgorithm(&F);
         la.print();
         return false;
       }

   }; 

}  

char LivenessAnalysisPass::ID = 0;
static RegisterPass<LivenessAnalysisPass> X("cse231-liveness", "Liveness analysis", false, false);
