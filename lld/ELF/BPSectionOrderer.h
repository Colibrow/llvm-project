//===- BPSectionOrderer.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file uses Balanced Partitioning to order sections to improve startup
/// time and compressed size.
///
//===----------------------------------------------------------------------===//

#ifndef LLD_ELF_BPSECTION_ORDERER_H
#define LLD_ELF_BPSECTION_ORDERER_H

#include "InputFiles.h"
#include "InputSection.h"
#include "Relocations.h"
#include "Symbols.h"
#include "lld/Common/SectionOrderer.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/xxhash.h"

namespace lld::elf {

class InputSection;

class ELFSymbol : public BPSymbol {
  const Symbol *sym;

public:
  explicit ELFSymbol(const Symbol *s) : sym(s) {}

  llvm::StringRef getName() const override { return sym->getName(); }

  BPSymbol *asDefinedSymbol() override {
    if (auto *d = llvm::dyn_cast<Defined>(sym))
      return this;
    return nullptr;
  }

  uint64_t getValue() const override {
    if (auto *d = llvm::dyn_cast<Defined>(sym))
      return d->value;
    return 0;
  }

  uint64_t getSize() const override {
    if (auto *d = llvm::dyn_cast<Defined>(sym))
      return d->size;
    return 0;
  }

  const Symbol *getSymbol() const { return sym; }
};

class ELFSection : public BPSectionBase {
  const InputSectionBase *isec;
  mutable std::vector<std::unique_ptr<ELFSymbol>> symbolCache;

public:
  explicit ELFSection(const InputSectionBase *sec) : isec(sec) {}

  const InputSectionBase *getSection() const { return isec; }

  llvm::StringRef getName() const override { return isec->name; }

  uint64_t getSize() const override { return isec->getSize(); }

  bool isCodeSection() const override {
    return isec->flags & llvm::ELF::SHF_EXECINSTR;
  }

  bool hasValidData() const override {
    return isec && !isec->content().empty();
  }

  llvm::ArrayRef<uint8_t> getSectionData() const override {
    return isec->content();
  }

  llvm::ArrayRef<BPSymbol *> getSymbols() const override {
    if (symbolCache.empty()) {
      for (Symbol *sym : isec->file->getSymbols())
        symbolCache.push_back(std::make_unique<ELFSymbol>(sym));
    }
    static std::vector<BPSymbol *> result;
    result.clear();
    for (const auto &sym : symbolCache)
      result.push_back(sym.get());
    return result;
  }

  void getSectionHash(llvm::SmallVectorImpl<uint64_t> &hashes,
                      const llvm::DenseMap<const BPSectionBase *, uint64_t>
                          &sectionToIdx) const override {
    constexpr unsigned windowSize = 4;

    // Convert BPSectionBase map to InputSection map
    llvm::DenseMap<const InputSectionBase *, uint64_t> elfSectionToIdx;
    for (const auto &[sec, idx] : sectionToIdx) {
      if (auto *elfSec = llvm::dyn_cast<ELFSection>(sec))
        elfSectionToIdx[elfSec->getSection()] = idx;
    }

    // Calculate content hashes
    for (size_t i = 0; i < isec->content().size(); i++) {
      auto window = isec->content().slice(i, windowSize);
      hashes.push_back(xxHash64(window));
    }

    // Calculate relocation hashes
    // for (const Relocation &r : isec->relocations) {
    //   if (r.offset >= isec->content().size())
    //     continue;

    //   uint64_t relocHash = getRelocHash(r, elfSectionToIdx);
    //   uint32_t start = (r.offset < windowSize) ? 0 : r.offset - windowSize +
    //   1; for (uint32_t i = start; i < r.offset + r.getSize(); i++) {
    //     auto window = isec->content().slice(i, windowSize);
    //     hashes.push_back(xxHash64(window) + relocHash);
    //   }
    // }

    llvm::sort(hashes);
    hashes.erase(std::unique(hashes.begin(), hashes.end()), hashes.end());
  }

  static bool classof(const BPSectionBase *s) {
    return true; // Since ELFSection is our only derived class
  }

private:
  //   static uint64_t getRelocHash(
  //       const Relocation &reloc,
  //       const llvm::DenseMap<const InputSection *, uint64_t> &sectionToIdx) {
  //     const Symbol *sym = reloc.sym;
  //     std::string kind = "R_" + llvm::toString(reloc.type);

  //     if (auto *d = llvm::dyn_cast<Defined>(sym)) {
  //       const InputSection *isec = d->section;
  //       if (!isec)
  //         return lld::getRelocHash(kind, 0, d->value, reloc.addend);

  //       auto it = sectionToIdx.find(isec);
  //       uint64_t sectionIdx = it != sectionToIdx.end() ? it->second : 0;
  //       return lld::getRelocHash(kind, sectionIdx, d->value, reloc.addend);
  //     }

  //     return lld::getRelocHash(kind, 0, 0, reloc.addend);
  //   }
};

/// Run Balanced Partitioning to find the optimal function and data order to
/// improve startup time and compressed size.
///
/// It is important that .subsections_via_symbols is used to ensure functions
/// and data are in their own sections and thus can be reordered.
llvm::DenseMap<const lld::elf::InputSectionBase *, int>
runBalancedPartitioning(Ctx &ctx,
                        llvm::StringRef profilePath,
                        bool forFunctionCompression, bool forDataCompression,
                        bool compressionSortStartupFunctions, bool verbose);
} // namespace lld::elf

#endif
