//===- BPSectionOrderer.cpp--------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPSectionOrderer.h"
#include "Config.h"
#include "InputFiles.h"
#include "InputSection.h"
#include "lld/Common/CommonLinkerContext.h"
#include "lld/Common/SectionOrderer.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/BalancedPartitioning.h"
#include "llvm/Support/TimeProfiler.h"

using namespace llvm;
using namespace lld::elf;

llvm::DenseMap<const lld::elf::InputSectionBase *, int>
lld::elf::runBalancedPartitioning(Ctx &ctx,
                                  llvm::StringRef profilePath,
                                  bool forFunctionCompression,
                                  bool forDataCompression,
                                  bool compressionSortStartupFunctions,
                                  bool verbose) {
  size_t highestAvailablePriority = std::numeric_limits<int>::max();
  SmallVector<lld::BPSectionBase *> sections;
  for (auto *isec : ctx.inputSections) {
    if (!isec || isec->content().empty())
      continue;
    sections.push_back(new ELFSection(isec));
  }

  auto baseMap = lld::SectionOrderer::reorderSectionsByBalancedPartitioning(
      highestAvailablePriority, profilePath, forFunctionCompression,
      forDataCompression, compressionSortStartupFunctions, verbose, sections);

  DenseMap<const InputSectionBase *, int> result;
  for (const auto &[baseSection, priority] : baseMap) {
    if (auto *elfSection = dyn_cast<ELFSection>(baseSection)) {
      result[elfSection->getSection()] = static_cast<int>(priority);
      delete elfSection;
    }
  }
  return result;
}
