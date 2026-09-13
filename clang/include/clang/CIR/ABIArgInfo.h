//==-- ABIArgInfo.h - Abstract info regarding ABI-specific arguments -------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines ABIArgInfo and associated types used by CIR to track information
// regarding ABI-coerced types for function arguments and return values. This
// was moved to the common library as it might be used by both CIRGen and
// passes.
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CIR_ABIARGINFO_H
#define CLANG_CIR_ABIARGINFO_H

#include "clang/AST/CharUnits.h"
#include "clang/CIR/MissingFeatures.h"
#include "mlir/IR/Types.h"

namespace cir {

class ABIArgInfo {
public:
  enum Kind : uint8_t {
    /// Pass the argument directly using the normal converted CIR type,
    /// or by coercing to another specified type stored in 'CoerceToType'). If
    /// an offset is specified (in UIntData), then the argument passed is offset
    /// by some number of bytes in the memory representation. A dummy argument
    /// is emitted before the real argument if the specified type stored in
    /// "PaddingType" is not zero.
    Direct,

    /// Pass the argument indirectly via a pointer.
    Indirect,

    /// Ignore the argument (treat as void). Useful for void and empty
    /// structs.
    Ignore,

    // TODO: more argument kinds will be added as the upstreaming proceeds.
  };

private:
  mlir::Type typeData;
  struct DirectAttrInfo {
    unsigned offset;
    unsigned align;
  };
  struct IndirectAttrInfo {
    unsigned align;
    unsigned addrSpace;
  };
  union {
    DirectAttrInfo directAttr;
    IndirectAttrInfo indirectAttr;
  };
  Kind theKind;
  bool sretAfterThis : 1;
  bool indirectByVal : 1;

public:
  ABIArgInfo(Kind k = Direct)
      : directAttr{0, 0}, theKind(k), sretAfterThis(false),
        indirectByVal(false) {}

  static ABIArgInfo getDirect(mlir::Type ty = nullptr) {
    ABIArgInfo info(Direct);
    info.setCoerceToType(ty);
    assert(!cir::MissingFeatures::abiArgInfo());
    return info;
  }

  static ABIArgInfo getIndirect(clang::CharUnits align, unsigned addrSpace = 0,
                                bool byVal = false) {
    ABIArgInfo ret(Indirect);
    ret.setIndirectAlign(align);
    ret.setIndirectAddrSpace(addrSpace);
    ret.setIndirectByVal(byVal);
    ret.setSRetAfterThis(false);
    return ret;
  }

  static ABIArgInfo getIgnore() { return ABIArgInfo(Ignore); }

  Kind getKind() const { return theKind; }
  bool isDirect() const { return theKind == Direct; }
  bool isIgnore() const { return theKind == Ignore; }
  bool isIndirect() const { return theKind == Indirect; }

  clang::CharUnits getIndirectAlign() const {
    assert(isIndirect() && "Invalid accessor!");
    return clang::CharUnits::fromQuantity(indirectAttr.align);
  }
  void setIndirectAlign(clang::CharUnits align) {
    assert(isIndirect() && "Invalid accessor!");
    indirectAttr.align = align.getQuantity();
  }
  unsigned getIndirectAddrSpace() const {
    assert(isIndirect() && "Invalid accessor!");
    return indirectAttr.addrSpace;
  }
  void setIndirectAddrSpace(unsigned as) {
    assert(isIndirect() && "Invalid accessor!");
    indirectAttr.addrSpace = as;
  }
  bool getIndirectByVal() const {
    assert(isIndirect() && "Invalid accessor!");
    return indirectByVal;
  }
  void setIndirectByVal(bool v) {
    assert(isIndirect() && "Invalid accessor!");
    indirectByVal = v;
  }
  bool isSRetAfterThis() const {
    assert(isIndirect() && "Invalid accessor!");
    return sretAfterThis;
  }
  void setSRetAfterThis(bool v) {
    assert(isIndirect() && "Invalid accessor!");
    sretAfterThis = v;
  }
  bool isExtend() const {
    assert(!cir::MissingFeatures::abiArgInfo());
    return false;
  }
  bool isNoExt() const {
    assert(!cir::MissingFeatures::abiArgInfo());
    return false;
  }
  bool isIndirectAliased() const {
    assert(!cir::MissingFeatures::abiArgInfo());
    return false;
  }

  bool canHaveCoerceToType() const {
    assert(!cir::MissingFeatures::abiArgInfo());
    return isDirect();
  }

  unsigned getDirectOffset() const {
    assert(!cir::MissingFeatures::abiArgInfo());
    return directAttr.offset;
  }

  mlir::Type getCoerceToType() const {
    assert(canHaveCoerceToType() && "invalid kind!");
    return typeData;
  }

  void setCoerceToType(mlir::Type ty) {
    assert(canHaveCoerceToType() && "invalid kind!");
    typeData = ty;
  }
};

} // namespace cir

#endif // CLANG_CIR_ABIARGINFO_H
