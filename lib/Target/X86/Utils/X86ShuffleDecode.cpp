//===-- X86ShuffleDecode.cpp - X86 shuffle decode logic -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Define several functions to decode x86 specific shuffle semantics into a
// generic vector mask.
//
//===----------------------------------------------------------------------===//

#include "X86ShuffleDecode.h"

//===----------------------------------------------------------------------===//
//  Vector Mask Decoding
//===----------------------------------------------------------------------===//

namespace llvm {

void DecodeINSERTPSMask(unsigned Imm, SmallVectorImpl<unsigned> &ShuffleMask) {
  // Defaults the copying the dest value.
  ShuffleMask.push_back(0);
  ShuffleMask.push_back(1);
  ShuffleMask.push_back(2);
  ShuffleMask.push_back(3);

  // Decode the immediate.
  unsigned ZMask = Imm & 15;
  unsigned CountD = (Imm >> 4) & 3;
  unsigned CountS = (Imm >> 6) & 3;

  // CountS selects which input element to use.
  unsigned InVal = 4+CountS;
  // CountD specifies which element of destination to update.
  ShuffleMask[CountD] = InVal;
  // ZMask zaps values, potentially overriding the CountD elt.
  if (ZMask & 1) ShuffleMask[0] = SM_SentinelZero;
  if (ZMask & 2) ShuffleMask[1] = SM_SentinelZero;
  if (ZMask & 4) ShuffleMask[2] = SM_SentinelZero;
  if (ZMask & 8) ShuffleMask[3] = SM_SentinelZero;
}

// <3,1> or <6,7,2,3>
void DecodeMOVHLPSMask(unsigned NElts,
                       SmallVectorImpl<unsigned> &ShuffleMask) {
  for (unsigned i = NElts/2; i != NElts; ++i)
    ShuffleMask.push_back(NElts+i);

  for (unsigned i = NElts/2; i != NElts; ++i)
    ShuffleMask.push_back(i);
}

// <0,2> or <0,1,4,5>
void DecodeMOVLHPSMask(unsigned NElts,
                       SmallVectorImpl<unsigned> &ShuffleMask) {
  for (unsigned i = 0; i != NElts/2; ++i)
    ShuffleMask.push_back(i);

  for (unsigned i = 0; i != NElts/2; ++i)
    ShuffleMask.push_back(NElts+i);
}

void DecodePSHUFMask(unsigned NElts, unsigned Imm,
                     SmallVectorImpl<unsigned> &ShuffleMask) {
  for (unsigned i = 0; i != NElts; ++i) {
    ShuffleMask.push_back(Imm % NElts);
    Imm /= NElts;
  }
}

void DecodePSHUFHWMask(unsigned Imm,
                       SmallVectorImpl<unsigned> &ShuffleMask) {
  ShuffleMask.push_back(0);
  ShuffleMask.push_back(1);
  ShuffleMask.push_back(2);
  ShuffleMask.push_back(3);
  for (unsigned i = 0; i != 4; ++i) {
    ShuffleMask.push_back(4+(Imm & 3));
    Imm >>= 2;
  }
}

void DecodePSHUFLWMask(unsigned Imm,
                       SmallVectorImpl<unsigned> &ShuffleMask) {
  for (unsigned i = 0; i != 4; ++i) {
    ShuffleMask.push_back((Imm & 3));
    Imm >>= 2;
  }
  ShuffleMask.push_back(4);
  ShuffleMask.push_back(5);
  ShuffleMask.push_back(6);
  ShuffleMask.push_back(7);
}

void DecodeSHUFPMask(EVT VT, unsigned Imm,
                     SmallVectorImpl<unsigned> &ShuffleMask) {
  unsigned NumElts = VT.getVectorNumElements();

  unsigned NumLanes = VT.getSizeInBits() / 128;
  unsigned NumLaneElts = NumElts / NumLanes;

  int NewImm = Imm;
  for (unsigned l = 0; l < NumLanes; ++l) {
    unsigned LaneStart = l * NumLaneElts;
    // Part that reads from dest.
    for (unsigned i = 0; i != NumLaneElts/2; ++i) {
      ShuffleMask.push_back(NewImm % NumLaneElts + LaneStart);
      NewImm /= NumLaneElts;
    }
    // Part that reads from src.
    for (unsigned i = 0; i != NumLaneElts/2; ++i) {
      ShuffleMask.push_back(NewImm % NumLaneElts + NumElts + LaneStart);
      NewImm /= NumLaneElts;
    }
    if (NumLaneElts == 4) NewImm = Imm; // reload imm
  }
}

void DecodeUNPCKHMask(EVT VT, SmallVectorImpl<unsigned> &ShuffleMask) {
  unsigned NumElts = VT.getVectorNumElements();

  // Handle 128 and 256-bit vector lengths. AVX defines UNPCK* to operate
  // independently on 128-bit lanes.
  unsigned NumLanes = VT.getSizeInBits() / 128;
  if (NumLanes == 0 ) NumLanes = 1;  // Handle MMX
  unsigned NumLaneElts = NumElts / NumLanes;

  for (unsigned s = 0; s < NumLanes; ++s) {
    unsigned Start = s * NumLaneElts + NumLaneElts/2;
    unsigned End   = s * NumLaneElts + NumLaneElts;
    for (unsigned i = Start; i != End; ++i) {
      ShuffleMask.push_back(i);          // Reads from dest/src1
      ShuffleMask.push_back(i+NumElts);  // Reads from src/src2
    }
  }
}

/// DecodeUNPCKLMask - This decodes the shuffle masks for unpcklps/unpcklpd
/// etc.  VT indicates the type of the vector allowing it to handle different
/// datatypes and vector widths.
void DecodeUNPCKLMask(EVT VT, SmallVectorImpl<unsigned> &ShuffleMask) {
  unsigned NumElts = VT.getVectorNumElements();

  // Handle 128 and 256-bit vector lengths. AVX defines UNPCK* to operate
  // independently on 128-bit lanes.
  unsigned NumLanes = VT.getSizeInBits() / 128;
  if (NumLanes == 0 ) NumLanes = 1;  // Handle MMX
  unsigned NumLaneElts = NumElts / NumLanes;

  for (unsigned s = 0; s < NumLanes; ++s) {
    unsigned Start = s * NumLaneElts;
    unsigned End   = s * NumLaneElts + NumLaneElts/2;
    for (unsigned i = Start; i != End; ++i) {
      ShuffleMask.push_back(i);          // Reads from dest/src1
      ShuffleMask.push_back(i+NumElts);  // Reads from src/src2
    }
  }
}

// DecodeVPERMILPMask - Decodes VPERMILPS/ VPERMILPD permutes for any 128-bit
// 32-bit or 64-bit elements. For 256-bit vectors, it's considered as two 128
// lanes. For VPERMILPS, referenced elements can't cross lanes and the mask of
// the first lane must be the same of the second.
void DecodeVPERMILPMask(EVT VT, unsigned Imm,
                        SmallVectorImpl<unsigned> &ShuffleMask) {
  unsigned NumElts = VT.getVectorNumElements();

  unsigned NumLanes = VT.getSizeInBits() / 128;
  unsigned NumLaneElts = NumElts / NumLanes;

  for (unsigned l = 0; l != NumLanes; ++l) {
    unsigned LaneStart = l*NumLaneElts;
    for (unsigned i = 0; i != NumLaneElts; ++i) {
      unsigned Idx = NumLaneElts == 4 ? (Imm >> (i*2)) & 0x3
                                      : (Imm >> (i+LaneStart)) & 0x1;
      ShuffleMask.push_back(Idx+LaneStart);
    }
  }
}

void DecodeVPERM2F128Mask(EVT VT, unsigned Imm,
                          SmallVectorImpl<unsigned> &ShuffleMask) {
  unsigned HalfSize = VT.getVectorNumElements()/2;
  unsigned FstHalfBegin = (Imm & 0x3) * HalfSize;
  unsigned SndHalfBegin = ((Imm >> 4) & 0x3) * HalfSize;

  for (int i = FstHalfBegin, e = FstHalfBegin+HalfSize; i != e; ++i)
    ShuffleMask.push_back(i);
  for (int i = SndHalfBegin, e = SndHalfBegin+HalfSize; i != e; ++i)
    ShuffleMask.push_back(i);
}

void DecodeVPERM2F128Mask(unsigned Imm,
                          SmallVectorImpl<unsigned> &ShuffleMask) {
  // VPERM2F128 is used by any 256-bit EVT, but X86InstComments only
  // has information about the instruction and not the types. So for
  // instruction comments purpose, assume the 256-bit vector is v4i64.
  return DecodeVPERM2F128Mask(MVT::v4i64, Imm, ShuffleMask);
}

} // llvm namespace
