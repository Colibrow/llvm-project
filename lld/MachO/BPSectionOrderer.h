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

#ifndef LLD_MACHO_BPSECTION_ORDERER_H
#define LLD_MACHO_BPSECTION_ORDERER_H

#include "InputSection.h"
#include "Relocations.h"
#include "Symbols.h"
#include "lld/Common/SectionOrderer.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/TinyPtrVector.h"

namespace lld::macho {

class InputSection;

class MachoSymbol : public BPSymbol {
  const Symbol *sym;

public:
  explicit MachoSymbol(const Symbol *s) : sym(s) {}

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

class MachoSection : public BPSectionBase {
  const InputSection *isec;
  mutable std::vector<std::unique_ptr<MachoSymbol>> symbolCache;

public:
  explicit MachoSection(const InputSection *sec) : isec(sec) {}

  const InputSection *getSection() const { return isec; }

  llvm::StringRef getName() const override { return isec->getName(); }

  uint64_t getSize() const override { return isec->getSize(); }

  bool isCodeSection() const override { return macho::isCodeSection(isec); }

  bool hasValidData() const override {
    return isec && !isec->data.empty() && isec->data.data();
  }

  llvm::ArrayRef<uint8_t> getSectionData() const override { return isec->data; }

  llvm::ArrayRef<BPSymbol *> getSymbols() const override {
    // Lazy initialization of symbol cache
    if (symbolCache.empty()) {
      for (const auto *sym : isec->symbols)
        symbolCache.push_back(std::make_unique<MachoSymbol>(sym));
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
    llvm::DenseMap<const InputSection *, uint64_t> machoSectionToIdx;
    for (const auto &[sec, idx] : sectionToIdx) {
      if (auto *machoSec = llvm::dyn_cast<MachoSection>(sec))
        machoSectionToIdx[machoSec->getInputSection()] = idx;
    }

    // Calculate content hashes
    for (size_t i = 0; i < isec->data.size(); i++) {
      auto window = isec->data.drop_front(i).take_front(windowSize);
      hashes.push_back(xxHash64(window));
    }

    // Calculate relocation hashes
    for (const auto &r : isec->relocs) {
      if (r.length == 0 || r.referent.isNull() || r.offset >= isec->data.size())
        continue;

      uint64_t relocHash = getRelocHash(r, machoSectionToIdx);
      uint32_t start = (r.offset < windowSize) ? 0 : r.offset - windowSize + 1;
      for (uint32_t i = start; i < r.offset + r.length; i++) {
        auto window = isec->data.drop_front(i).take_front(windowSize);
        hashes.push_back(xxHash64(window) + relocHash);
      }
    }

    llvm::sort(hashes);
    hashes.erase(std::unique(hashes.begin(), hashes.end()), hashes.end());
  }

  const InputSection *getInputSection() const { return isec; }

  static bool classof(const BPSectionBase *s) {
    return true; // Since MachoSection is our only derived class
  }

private:
  static uint64_t getRelocHash(
      const Reloc &reloc,
      const llvm::DenseMap<const InputSection *, uint64_t> &sectionToIdx) {
    auto *isec = reloc.getReferentInputSection();
    std::optional<uint64_t> sectionIdx;
    auto sectionIdxIt = sectionToIdx.find(isec);
    if (sectionIdxIt != sectionToIdx.end())
      sectionIdx = sectionIdxIt->getSecond();

    std::string kind;
    if (isec)
      kind = ("Section " + Twine(isec->kind())).str();

    if (auto *sym = reloc.referent.dyn_cast<Symbol *>()) {
      kind += (" Symbol " + Twine(sym->kind())).str();
      if (auto *d = llvm::dyn_cast<Defined>(sym)) {
        if (llvm::isa_and_nonnull<CStringInputSection>(isec))
          return BPSectionBase::getRelocHash(kind, 0, isec->getOffset(d->value),
                                   reloc.addend);
        return BPSectionBase::getRelocHash(kind, sectionIdx.value_or(0), d->value,
                                 reloc.addend);
      }
    }
    return BPSectionBase::getRelocHash(kind, sectionIdx.value_or(0), 0,
                                     reloc.addend);
  }
};

/// Run Balanced Partitioning to find the optimal function and data order to
/// improve startup time and compressed size.
///
/// It is important that .subsections_via_symbols is used to ensure functions
/// and data are in their own sections and thus can be reordered.
llvm::DenseMap<const lld::macho::InputSection *, size_t>
runBalancedPartitioning(size_t &highestAvailablePriority,
                        llvm::StringRef profilePath,
                        bool forFunctionCompression, bool forDataCompression,
                        bool compressionSortStartupFunctions, bool verbose);

} // namespace lld::macho

#endif
