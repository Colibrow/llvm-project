//===- SectionOrderer.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the common interfaces which may be used by
// BPSectionOrderer.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_COMMON_SECTION_ORDERER_H
#define LLD_COMMON_SECTION_ORDERER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/xxhash.h"

namespace lld {

class BPSymbol {

public:
  virtual ~BPSymbol() = default;
  virtual llvm::StringRef getName() const = 0;
  virtual BPSymbol *asDefinedSymbol() = 0;
  virtual uint64_t getValue() const = 0;
  virtual uint64_t getSize() const = 0;
};

class BPSectionBase {
public:
  virtual ~BPSectionBase() = default;
  virtual llvm::StringRef getName() const = 0;
  virtual uint64_t getSize() const = 0;
  virtual bool hasValidData() const = 0;
  virtual bool isCodeSection() const = 0;
  virtual llvm::ArrayRef<uint8_t> getSectionData() const = 0;
  virtual llvm::ArrayRef<BPSymbol *> getSymbols() const = 0;
  virtual void
  getSectionHash(llvm::SmallVectorImpl<uint64_t> &hashes,
                 const llvm::DenseMap<const BPSectionBase *, uint64_t>
                     &sectionToIdx) const = 0;
  static llvm::StringRef getRootSymbol(llvm::StringRef Name) {
    auto [P0, S0] = Name.rsplit(".llvm.");
    auto [P1, S1] = P0.rsplit(".__uniq.");
    return P1;
  }

  static uint64_t getRelocHash(llvm::StringRef kind, uint64_t sectionIdx,
                               uint64_t offset, uint64_t addend) {
    return llvm::xxHash64((kind + ": " + llvm::Twine::utohexstr(sectionIdx) +
                           " + " + llvm::Twine::utohexstr(offset) + " + " +
                           llvm::Twine::utohexstr(addend))
                              .str());
  }
};

class SectionOrderer {
public:
  static llvm::DenseMap<const BPSectionBase *, size_t>
  reorderSectionsByBalancedPartitioning(
      size_t &highestAvailablePriority, llvm::StringRef profilePath,
      bool forFunctionCompression, bool forDataCompression,
      bool compressionSortStartupFunctions, bool verbose,
      llvm::SmallVector<BPSectionBase *> inputSections);
};

} // namespace lld

#endif
