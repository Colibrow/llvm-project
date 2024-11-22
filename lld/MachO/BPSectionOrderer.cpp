//===- BPSectionOrderer.cpp--------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BPSectionOrderer.h"
#include "InputSection.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/BalancedPartitioning.h"
#include "llvm/Support/TimeProfiler.h"

using namespace llvm;
using namespace lld::macho;

DenseMap<const InputSection *, size_t> lld::macho::runBalancedPartitioning(
    size_t &highestAvailablePriority, StringRef profilePath,
    bool forFunctionCompression, bool forDataCompression,
    bool compressionSortStartupFunctions, bool verbose) {

  SmallVector<BPSectionBase *> sections;
  for (const auto *file : inputFiles) {
    for (auto *sec : file->sections) {
      for (auto &subsec : sec->subsections) {
        auto *isec = subsec.isec;
        if (!isec || isec->data.empty() || !isec->data.data())
          continue;
        sections.push_back(new MachoSection(isec));
      }
    }
  }

  auto baseMap = lld::SectionOrderer::reorderSectionsByBalancedPartitioning(
      highestAvailablePriority, profilePath, forFunctionCompression,
      forDataCompression, compressionSortStartupFunctions, verbose, sections);

  DenseMap<const InputSection *, size_t> result;
  for (const auto &[baseSection, priority] : baseMap) {
    if (auto *machoSection = dyn_cast<MachoSection>(baseSection)) {
      result[machoSection->getSection()] = priority;
      delete machoSection;
    }
  }
  return result;
}
