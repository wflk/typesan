//===- RegionInfo.cpp - SESE region detection analysis --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Detects single entry single exit regions in the control flow graph.
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/RegionInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/RegionInfoImpl.h"
#include "llvm/Analysis/RegionIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
#include <iterator>
#include <set>
#ifndef NDEBUG
#include "llvm/Analysis/RegionPrinter.h"
#endif

using namespace llvm;

#define DEBUG_TYPE "region"

namespace llvm {
template class RegionBase<RegionTraits<Function>>;
template class RegionNodeBase<RegionTraits<Function>>;
template class RegionInfoBase<RegionTraits<Function>>;
}

STATISTIC(numRegions,       "The # of regions");
STATISTIC(numSimpleRegions, "The # of simple regions");

// Always verify if expensive checking is enabled.

static cl::opt<bool,true>
VerifyRegionInfoX(
  "verify-region-info",
  cl::location(RegionInfoBase<RegionTraits<Function>>::VerifyRegionInfo),
  cl::desc("Verify region info (time consuming)"));


static cl::opt<Region::PrintStyle, true> printStyleX("print-region-style",
  cl::location(RegionInfo::printStyle),
  cl::Hidden,
  cl::desc("style of printing regions"),
  cl::values(
    clEnumValN(Region::PrintNone, "none",  "print no details"),
    clEnumValN(Region::PrintBB, "bb",
               "print regions in detail with block_iterator"),
    clEnumValN(Region::PrintRN, "rn",
               "print regions in detail with element_iterator"),
    clEnumValEnd));


//===----------------------------------------------------------------------===//
// Region implementation
//

Region::Region(BasicBlock *Entry, BasicBlock *Exit,
               RegionInfo* RI,
               DominatorTree *DT, Region *Parent) :
  RegionBase<RegionTraits<Function>>(Entry, Exit, RI, DT, Parent) {

}

Region::~Region() { }

//===----------------------------------------------------------------------===//
// RegionInfo implementation
//

RegionInfo::RegionInfo() :
  RegionInfoBase<RegionTraits<Function>>() {

}

RegionInfo::~RegionInfo() {

}

void RegionInfo::updateStatistics(Region *R) {
  ++numRegions;

  // TODO: Slow. Should only be enabled if -stats is used.
  if (R->isSimple())
    ++numSimpleRegions;
}

void RegionInfo::recalculate(Function &F, DominatorTree *DT_,
                             PostDominatorTree *PDT_, DominanceFrontier *DF_) {
  DT = DT_;
  PDT = PDT_;
  DF = DF_;

  TopLevelRegion = new Region(&F.getEntryBlock(), nullptr,
                              this, DT, nullptr);
  updateStatistics(TopLevelRegion);
  calculate(F);
}

#ifndef NDEBUG
void RegionInfo::view() { viewRegion(this); }

void RegionInfo::viewOnly() { viewRegionOnly(this); }
#endif

//===----------------------------------------------------------------------===//
// RegionInfoPass implementation
//

RegionInfoPass::RegionInfoPass() : FunctionPass(ID) {
  initializeRegionInfoPassPass(*PassRegistry::getPassRegistry());
}

RegionInfoPass::~RegionInfoPass() {

}

bool RegionInfoPass::runOnFunction(Function &F) {
  releaseMemory();

  auto DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  auto PDT = &getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
  auto DF = &getAnalysis<DominanceFrontierWrapperPass>().getDominanceFrontier();

  RI.recalculate(F, DT, PDT, DF);
  return false;
}

void RegionInfoPass::releaseMemory() {
  RI.releaseMemory();
}

void RegionInfoPass::verifyAnalysis() const {
    RI.verifyAnalysis();
}

void RegionInfoPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequiredTransitive<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<DominanceFrontierWrapperPass>();
}

void RegionInfoPass::print(raw_ostream &OS, const Module *) const {
  RI.print(OS);
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void RegionInfoPass::dump() const {
  RI.dump();
}
#endif

char RegionInfoPass::ID = 0;

INITIALIZE_PASS_BEGIN(RegionInfoPass, "regions",
                "Detect single entry single exit regions", true, true)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominanceFrontierWrapperPass)
INITIALIZE_PASS_END(RegionInfoPass, "regions",
                "Detect single entry single exit regions", true, true)

// Create methods available outside of this file, to use them
// "include/llvm/LinkAllPasses.h". Otherwise the pass would be deleted by
// the link time optimization.

namespace llvm {
  FunctionPass *createRegionInfoPass() {
    return new RegionInfoPass();
  }
}

//===----------------------------------------------------------------------===//
// RegionInfoAnalysis implementation
//

char RegionInfoAnalysis::PassID;

RegionInfo RegionInfoAnalysis::run(Function &F, AnalysisManager<Function> &AM) {
  RegionInfo RI;
  auto *DT = &AM.getResult<DominatorTreeAnalysis>(F);
  auto *PDT = &AM.getResult<PostDominatorTreeAnalysis>(F);
  auto *DF = &AM.getResult<DominanceFrontierAnalysis>(F);

  RI.recalculate(F, DT, PDT, DF);
  return RI;
}

RegionInfoPrinterPass::RegionInfoPrinterPass(raw_ostream &OS)
  : OS(OS) {}

PreservedAnalyses RegionInfoPrinterPass::run(Function &F,
                                             FunctionAnalysisManager &AM) {
  OS << "Region Tree for function: " << F.getName() << "\n";
  AM.getResult<RegionInfoAnalysis>(F).print(OS);

  return PreservedAnalyses::all();
}

PreservedAnalyses RegionInfoVerifierPass::run(Function &F,
                                              AnalysisManager<Function> &AM) {
  AM.getResult<RegionInfoAnalysis>(F).verifyAnalysis();

  return PreservedAnalyses::all();
}
