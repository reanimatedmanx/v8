// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_FRAME_CONSTANTS_H_
#define V8_EXECUTION_FRAME_CONSTANTS_H_

#include "src/common/globals.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

// Every pointer in a frame has a slot id. On 32-bit platforms, doubles consume
// two slots.
//
// Stack slot indices >= 0 access the callee stack with slot 0 corresponding to
// the callee's saved return address and 1 corresponding to the saved frame
// pointer. Some frames have additional information stored in the fixed header,
// for example JSFunctions store the function context and marker in the fixed
// header, with slot index 2 corresponding to the current function context and 3
// corresponding to the frame marker/JSFunction.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter n   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |  parameter n-1  |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |   parameter 1   |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter 0   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |Context/Frm. Type|   v   if a constant pool   |
//       |-----------------+----    is used, cp = 1,    |
// 3+cp  |                 |   ^   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |                 |   |                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |                 | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class CommonFrameConstants : public AllStatic {
 public:
  static constexpr int kCallerFPOffset = 0 * kSystemPointerSize;
  static constexpr int kCallerPCOffset = kCallerFPOffset + 1 * kFPOnStackSize;
  static constexpr int kCallerSPOffset = kCallerPCOffset + 1 * kPCOnStackSize;

  // Fixed part of the frame consists of return address, caller fp,
  // constant pool (if V8_EMBEDDED_CONSTANT_POOL_BOOL), context, and
  // function. CommonFrame::IterateExpressions assumes that kLastObjectOffset
  // is the last object pointer.
  static constexpr int kFixedFrameSizeAboveFp = kPCOnStackSize + kFPOnStackSize;
  static constexpr int kFixedSlotCountAboveFp =
      kFixedFrameSizeAboveFp / kSystemPointerSize;
  static constexpr int kCPSlotSize =
      V8_EMBEDDED_CONSTANT_POOL_BOOL ? kSystemPointerSize : 0;
  static constexpr int kCPSlotCount = kCPSlotSize / kSystemPointerSize;
  static constexpr int kConstantPoolOffset =
      kCPSlotSize ? -1 * kSystemPointerSize : 0;
  static constexpr int kContextOrFrameTypeSize = kSystemPointerSize;
  static constexpr int kContextOrFrameTypeOffset =
      -(kCPSlotSize + kContextOrFrameTypeSize);
};

// StandardFrames are used for both unoptimized and optimized JavaScript
// frames. They always have a context below the saved fp/constant
// pool, below that the JSFunction of the executing function and below that an
// integer (not a Smi) containing the actual number of arguments passed to the
// JavaScript code.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter n   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |  parameter n-1  |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |   parameter 1   |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter 0   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |     Context     |   |   if a constant pool   |
//       |- - - - - - - - -|   |    is used, cp = 1,    |
// 3+cp  |    JSFunction   |   |   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |      argc       |   v                        |
//       +-----------------+----                        |
// 5+cp  |  expressions or |   ^                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |  pushed values  | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class StandardFrameConstants : public CommonFrameConstants {
 public:
  static constexpr int kFixedFrameSizeFromFp =
      3 * kSystemPointerSize + kCPSlotSize;
  static constexpr int kFixedFrameSize =
      kFixedFrameSizeAboveFp + kFixedFrameSizeFromFp;
  static constexpr int kFixedSlotCountFromFp =
      kFixedFrameSizeFromFp / kSystemPointerSize;
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize;
  static constexpr int kContextOffset = kContextOrFrameTypeOffset;
  static constexpr int kFunctionOffset = -2 * kSystemPointerSize - kCPSlotSize;
  static constexpr int kArgCOffset = -3 * kSystemPointerSize - kCPSlotSize;
  static constexpr int kExpressionsOffset =
      -4 * kSystemPointerSize - kCPSlotSize;
  static constexpr int kFirstPushedFrameValueOffset = kExpressionsOffset;
  static constexpr int kLastObjectOffset = kContextOffset;
};

// TypedFrames have a type maker value below the saved FP/constant pool to
// distinguish them from StandardFrames, which have a context in that position
// instead.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter n   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |  parameter n-1  |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |   parameter 1   |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter 0   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |Frame Type Marker|   v   if a constant pool   |
//       |-----------------+----    is used, cp = 1,    |
// 3+cp  |  pushed value 0 |   ^   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |  pushed value 1 |   |                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |                 | Frame slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//       |                 |   v                        |
//  -----+-----------------+----- <-- stack ptr -------------
//
class TypedFrameConstants : public CommonFrameConstants {
 public:
  static constexpr int kFrameTypeSize = kContextOrFrameTypeSize;
  static constexpr int kFrameTypeOffset = kContextOrFrameTypeOffset;
  static constexpr int kFixedFrameSizeFromFp = kCPSlotSize + kFrameTypeSize;
  static constexpr int kFixedSlotCountFromFp =
      kFixedFrameSizeFromFp / kSystemPointerSize;
  static constexpr int kFixedFrameSize =
      StandardFrameConstants::kFixedFrameSizeAboveFp + kFixedFrameSizeFromFp;
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize;
  static constexpr int kFirstPushedFrameValueOffset =
      -kFixedFrameSizeFromFp - kSystemPointerSize;
};

#define FRAME_PUSHED_VALUE_OFFSET(parent, x) \
  (parent::kFirstPushedFrameValueOffset - (x)*kSystemPointerSize)
#define FRAME_SIZE(parent, count) \
  (parent::kFixedFrameSize + (count)*kSystemPointerSize)
#define FRAME_SIZE_FROM_FP(parent, count) \
  (parent::kFixedFrameSizeFromFp + (count)*kSystemPointerSize)
#define DEFINE_FRAME_SIZES(parent, count)                                      \
  static constexpr int kFixedFrameSize = FRAME_SIZE(parent, count);            \
  static constexpr int kFixedSlotCount = kFixedFrameSize / kSystemPointerSize; \
  static constexpr int kFixedFrameSizeFromFp =                                 \
      FRAME_SIZE_FROM_FP(parent, count);                                       \
  static constexpr int kFixedSlotCountFromFp =                                 \
      kFixedFrameSizeFromFp / kSystemPointerSize;                              \
  static constexpr int kExtraSlotCount =                                       \
      kFixedFrameSize / kSystemPointerSize -                                   \
      parent::kFixedFrameSize / kSystemPointerSize

#define STANDARD_FRAME_EXTRA_PUSHED_VALUE_OFFSET(x) \
  FRAME_PUSHED_VALUE_OFFSET(StandardFrameConstants, x)
#define DEFINE_STANDARD_FRAME_SIZES(count) \
  DEFINE_FRAME_SIZES(StandardFrameConstants, count)

#define TYPED_FRAME_PUSHED_VALUE_OFFSET(x) \
  FRAME_PUSHED_VALUE_OFFSET(TypedFrameConstants, x)
#define DEFINE_TYPED_FRAME_SIZES(count) \
  DEFINE_FRAME_SIZES(TypedFrameConstants, count)

class BuiltinFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kFunctionOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLengthOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);
};

// Fixed frame slots shared by the js-to-wasm wrapper, the
// ReturnPromiseOnSuspend wrapper and the WasmResume wrapper.
class BuiltinWasmWrapperConstants : public TypedFrameConstants {
 public:
  // This slot contains the number of slots at the top of the frame that need to
  // be scanned by the GC.
  static constexpr int kGCScanSlotCountOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  // The number of parameters passed to this function.
  static constexpr int kInParamCountOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  // The number of parameters according to the signature.
  static constexpr int kParamCountOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  static constexpr int kSuspenderOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(3);
  static constexpr int kFunctionDataOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(4);
};

class ConstructFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kContextOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLengthOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kConstructorOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  static constexpr int kPaddingOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(3);
  static constexpr int kNewTargetOrImplicitReceiverOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(4);
  DEFINE_TYPED_FRAME_SIZES(5);
};

class FastConstructFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kContextOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kImplicitReceiverOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);
};

#if V8_ENABLE_WEBASSEMBLY
class CWasmEntryFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative:
  static constexpr int kCEntryFPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  DEFINE_TYPED_FRAME_SIZES(1);
};

class WasmFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kWasmInstanceOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  DEFINE_TYPED_FRAME_SIZES(1);
};

class WasmExitFrameConstants : public WasmFrameConstants {
 public:
  // FP-relative.
  static const int kCallingPCOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  DEFINE_TYPED_FRAME_SIZES(2);
};

class JSToWasmWrapperConstants {
 public:
  // FP-relative.
  static constexpr int kResultArrayOffset = 2 * kSystemPointerSize;
  static constexpr int kInstanceOffset = 3 * kSystemPointerSize;

  // Offsets into the wrapper buffer for values passed from Torque to the
  // assembly builtin.
  static constexpr size_t kWrapperBufferReturnCount = 0;
  static constexpr size_t kWrapperBufferRefReturnCount = 4;
  static constexpr size_t kWrapperBufferSigRepresentationArray = 8;
  static constexpr size_t kWrapperBufferStackReturnBufferSize = 16;
  static constexpr size_t kWrapperBufferCallTarget = 24;
  static constexpr size_t kWrapperBufferParamStart = 32;
  static constexpr size_t kWrapperBufferParamEnd = 40;

  // Offsets into the wrapper buffer for values passed from the assembly builtin
  // to Torque.
  static constexpr size_t kWrapperBufferStackReturnBufferStart = 16;
  static constexpr size_t kWrapperBufferFPReturnRegister1 = 24;
  static constexpr size_t kWrapperBufferFPReturnRegister2 = 32;
  static constexpr size_t kWrapperBufferGPReturnRegister1 = 40;
  static constexpr size_t kWrapperBufferGPReturnRegister2 =
      kWrapperBufferGPReturnRegister1 + kSystemPointerSize;

  // Size of the wrapper buffer
  static constexpr int kWrapperBufferSize =
      kWrapperBufferGPReturnRegister2 + kSystemPointerSize;
};

#endif  // V8_ENABLE_WEBASSEMBLY

class BuiltinContinuationFrameConstants : public TypedFrameConstants {
 public:
  // FP-relative.
  static constexpr int kFunctionOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kFrameSPtoFPDeltaAtDeoptimize =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(1);
  static constexpr int kBuiltinContextOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(2);
  static constexpr int kBuiltinIndexOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(3);

  // The argument count is in the first allocatable register, stored below the
  // fixed part of the frame and therefore is not part of the fixed frame size.
  static constexpr int kArgCOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(4);
  DEFINE_TYPED_FRAME_SIZES(4);

  // Returns the number of padding stack slots needed when we have
  // 'register_count' register slots.
  // This is needed on some architectures to ensure the stack pointer is
  // aligned.
  static int PaddingSlotCount(int register_count);
};

class ExitFrameConstants : public TypedFrameConstants {
 public:
  static constexpr int kSPOffset = TYPED_FRAME_PUSHED_VALUE_OFFSET(0);
  static constexpr int kLastExitFrameField = kSPOffset;
  DEFINE_TYPED_FRAME_SIZES(1);

  // FP-relative displacement of the caller's SP.  It points just
  // below the saved PC.
  static constexpr int kCallerSPDisplacement = kCallerSPOffset;
};

// Behaves like an exit frame but with target, new target and arguments count
// args.
class BuiltinExitFrameConstants : public ExitFrameConstants {
 public:
  static constexpr int kNewTargetOffset =
      kCallerPCOffset + 1 * kSystemPointerSize;
  static constexpr int kTargetOffset =
      kNewTargetOffset + 1 * kSystemPointerSize;
  static constexpr int kArgcOffset = kTargetOffset + 1 * kSystemPointerSize;
  static constexpr int kPaddingOffset = kArgcOffset + 1 * kSystemPointerSize;
  static constexpr int kFirstArgumentOffset =
      kPaddingOffset + 1 * kSystemPointerSize;
  static constexpr int kNumExtraArgsWithoutReceiver = 4;
  static constexpr int kNumExtraArgsWithReceiver =
      kNumExtraArgsWithoutReceiver + 1;
};

// Behaves like an exit frame but with target and arguments count args followed
// by v8::FunctionCallbackInfo's implicit arguments, followed by JS arguments
// passed to the JS function (receiver and etc.).
class ApiCallbackExitFrameConstants : public ExitFrameConstants {
 public:
  // The following two constants must be in sync with v8::FunctionCallbackInfo's
  // layout.
  static constexpr int kFunctionCallbackInfoNewTargetIndex = 5;
  static constexpr int kFunctionCallbackInfoArgsLength = 6;

  // Target, argc, context and optional padding (for arm64).
  static constexpr int kTargetOffset = kCallerPCOffset + 1 * kSystemPointerSize;
  static constexpr int kArgcOffset = kTargetOffset + 1 * kSystemPointerSize;
  static constexpr int kContextOffset = kArgcOffset + 1 * kSystemPointerSize;
  static constexpr int kOptionalPaddingOffset =
      kContextOffset + 1 * kSystemPointerSize;

#if V8_TARGET_ARCH_ARM64
  // Padding is required to keep the stack 16-byte aligned.
  static constexpr int kOptionalPaddingSize = kSystemPointerSize;
  static constexpr int kAdditionalParametersCount = 4;
#else
  static constexpr int kOptionalPaddingSize = 0;
  static constexpr int kAdditionalParametersCount = 3;
#endif  // V8_TARGET_ARCH_ARM64

  // FunctionCallbackInfo.
  static constexpr int kFunctionCallbackInfoOffset =
      kOptionalPaddingOffset + kOptionalPaddingSize;
  static constexpr int kNewTargetOffset =
      kFunctionCallbackInfoOffset +
      kFunctionCallbackInfoNewTargetIndex * kSystemPointerSize;
  // JS arguments.
  static constexpr int kFirstArgumentOffset =
      kFunctionCallbackInfoOffset +
      kFunctionCallbackInfoArgsLength * kSystemPointerSize;
};

// Unoptimized frames are used for interpreted and baseline-compiled JavaScript
// frames. They are a "standard" frame, with an additional fixed header for the
// BytecodeArray, bytecode offset (if running interpreted), feedback vector (if
// running baseline code), and then the interpreter register file.
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter n   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |  parameter n-1  |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |   parameter 1   |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter 0   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   | [Constant Pool] |   |                        |
//       |- - - - - - - - -|   |                        |
// 2+cp  |     Context     |   |   if a constant pool   |
//       |- - - - - - - - -|   |    is used, cp = 1,    |
// 3+cp  |    JSFunction   |   |   otherwise, cp = 0    |
//       |- - - - - - - - -|   |                        |
// 4+cp  |      argc       |   v                        |
//       +-----------------+----                        |
// 5+cp  |  BytecodeArray  |   ^                        |
//       |- - - - - - - - -| Unoptimized code header    |
// 6+cp  |  offset or FBV  |   v                        |
//       +-----------------+----                        |
// 7+cp  |   register 0    |   ^                     Callee
//       |- - - - - - - - -|   |                   frame slots
// 8+cp  |   register 1    | Register file         (slot >= 0)
//  ...  |       ...       |   |                        |
//       |  register n-1   |   |                        |
//       |- - - - - - - - -|   |                        |
// 8+cp+n|   register n    |   v                        v
//  -----+-----------------+----- <-- stack ptr -------------
//
class UnoptimizedFrameConstants : public StandardFrameConstants {
 public:
  // FP-relative.
  static constexpr int kBytecodeArrayFromFp =
      STANDARD_FRAME_EXTRA_PUSHED_VALUE_OFFSET(0);
  static constexpr int kBytecodeOffsetOrFeedbackVectorFromFp =
      STANDARD_FRAME_EXTRA_PUSHED_VALUE_OFFSET(1);
  DEFINE_STANDARD_FRAME_SIZES(2);

  static constexpr int kFirstParamFromFp =
      StandardFrameConstants::kCallerSPOffset;
  static constexpr int kRegisterFileFromFp =
      -kFixedFrameSizeFromFp - kSystemPointerSize;
  static constexpr int kExpressionsOffset = kRegisterFileFromFp;

  // Expression index for {JavaScriptFrame::GetExpressionAddress}.
  static constexpr int kBytecodeArrayExpressionIndex = -2;
  static constexpr int kBytecodeOffsetOrFeedbackVectorExpressionIndex = -1;
  static constexpr int kRegisterFileExpressionIndex = 0;

  // Returns the number of stack slots needed for 'register_count' registers.
  // This is needed because some architectures must pad the stack frame with
  // additional stack slots to ensure the stack pointer is aligned.
  static int RegisterStackSlotCount(int register_count);
};

// Interpreter frames are unoptimized frames that are being executed by the
// interpreter. In this case, the "offset or FBV" slot contains the bytecode
// offset of the currently executing bytecode.
class InterpreterFrameConstants : public UnoptimizedFrameConstants {
 public:
  static constexpr int kBytecodeOffsetExpressionIndex =
      kBytecodeOffsetOrFeedbackVectorExpressionIndex;

  static constexpr int kBytecodeOffsetFromFp =
      kBytecodeOffsetOrFeedbackVectorFromFp;
};

// Sparkplug frames are unoptimized frames that are being executed by
// sparkplug-compiled baseline code. base. In this case, the "offset or FBV"
// slot contains a cached pointer to the feedback vector.
class BaselineFrameConstants : public UnoptimizedFrameConstants {
 public:
  static constexpr int kFeedbackVectorExpressionIndex =
      kBytecodeOffsetOrFeedbackVectorExpressionIndex;

  static constexpr int kFeedbackVectorFromFp =
      kBytecodeOffsetOrFeedbackVectorFromFp;
};

inline static int FPOffsetToFrameSlot(int frame_offset) {
  return StandardFrameConstants::kFixedSlotCountAboveFp - 1 -
         frame_offset / kSystemPointerSize;
}

inline static int FrameSlotToFPOffset(int slot) {
  return (StandardFrameConstants::kFixedSlotCountAboveFp - 1 - slot) *
         kSystemPointerSize;
}

}  // namespace internal
}  // namespace v8

#if V8_TARGET_ARCH_IA32
#include "src/execution/ia32/frame-constants-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/execution/x64/frame-constants-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/execution/arm64/frame-constants-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/execution/arm/frame-constants-arm.h"
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/execution/ppc/frame-constants-ppc.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/execution/mips64/frame-constants-mips64.h"
#elif V8_TARGET_ARCH_LOONG64
#include "src/execution/loong64/frame-constants-loong64.h"
#elif V8_TARGET_ARCH_S390
#include "src/execution/s390/frame-constants-s390.h"
#elif V8_TARGET_ARCH_RISCV32 || V8_TARGET_ARCH_RISCV64
#include "src/execution/riscv/frame-constants-riscv.h"
#else
#error Unsupported target architecture.
#endif

#endif  // V8_EXECUTION_FRAME_CONSTANTS_H_
