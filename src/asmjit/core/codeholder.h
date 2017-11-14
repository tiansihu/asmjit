// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_CODEHOLDER_H
#define _ASMJIT_CORE_CODEHOLDER_H

// [Dependencies]
#include "../core/arch.h"
#include "../core/intutils.h"
#include "../core/memutils.h"
#include "../core/operand.h"
#include "../core/simdtypes.h"
#include "../core/smallstring.h"
#include "../core/target.h"
#include "../core/zone.h"
#include "../core/zonehash.h"
#include "../core/zonevector.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class CodeEmitter;
class CodeHolder;
class Logger;

// ============================================================================
// [asmjit::AlignMode]
// ============================================================================

//! Align mode.
enum AlignMode : uint32_t {
  kAlignCode  = 0,                       //!< Align executable code.
  kAlignData  = 1,                       //!< Align non-executable code.
  kAlignZero  = 2,                       //!< Align by a sequence of zeros.
  kAlignCount = 3                        //!< Count of alignment modes.
};

// ============================================================================
// [asmjit::ErrorHandler]
// ============================================================================

//! Error handler can be used to override the default behavior of error handling
//! available to all classes that inherit \ref CodeEmitter. See \ref handleError().
class ASMJIT_VIRTAPI ErrorHandler {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new `ErrorHandler` instance.
  ASMJIT_API ErrorHandler() noexcept;
  //! Destroy the `ErrorHandler` instance.
  ASMJIT_API virtual ~ErrorHandler() noexcept;

  // --------------------------------------------------------------------------
  // [Handle Error]
  // --------------------------------------------------------------------------

  //! Error handler (must be reimplemented).
  //!
  //! Error handler is called after an error happened and before it's propagated
  //! to the caller. There are multiple ways how the error handler can be used:
  //!
  //! 1. User-based error handling without throwing exception or using C's
  //!    `longjmp()`. This is for users that don't use exceptions and want
  //!    customized error handling.
  //!
  //! 2. Throwing an exception. AsmJit doesn't use exceptions and is completely
  //!    exception-safe, but you can throw exception from your error handler if
  //!    this way is the preferred way of handling errors in your project.
  //!
  //! 3. Using plain old C's `setjmp()` and `longjmp()`. Asmjit always puts
  //!    `CodeEmitter` to a consistent state before calling `handleError()`
  //!    so `longjmp()` can be used without any issues to cancel the code
  //!    generation if an error occurred. There is no difference between
  //!    exceptions and `longjmp()` from AsmJit's perspective, however,
  //!    never jump outside of `CodeHolder` and `CodeEmitter` scope as you
  //!    would leak memory.
  virtual void handleError(Error err, const char* message, CodeEmitter* origin) = 0;
};

// ============================================================================
// [asmjit::CodeBuffer]
// ============================================================================

//! Code or data buffer.
struct CodeBuffer {
  enum Flags : uint32_t {
    kFlagIsExternal       = 0x00000001U, //!< Buffer is external (not allocated by asmjit).
    kFlagIsFixed          = 0x00000002U  //!< Buffer is fixed (cannot be reallocated).
  };

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint32_t getFlags() const noexcept { return _flags; }
  inline bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }

  inline bool isFixed() const noexcept { return hasFlag(kFlagIsFixed); }
  inline bool isExternal() const noexcept { return hasFlag(kFlagIsExternal); }

  inline bool hasData() const noexcept { return _data != nullptr; }
  inline uint8_t* getData() noexcept { return _data; }
  inline const uint8_t* getData() const noexcept { return _data; }

  inline size_t getLength() const noexcept { return _length; }
  inline size_t getCapacity() const noexcept { return _capacity; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t* _data;                        //!< The content of the buffer (data).
  size_t _length;                        //!< Number of bytes of `data` used.
  size_t _capacity;                      //!< Buffer capacity (in bytes).
  uint32_t _flags;                       //!< Buffer flags.
};

// ============================================================================
// [asmjit::SectionEntry]
// ============================================================================

//! Section entry.
class SectionEntry {
public:
  enum Id : uint32_t {
    kInvalidId       = 0xFFFFFFFFU       //!< Invalid section id.
  };

  //! Section flags.
  enum Flags : uint32_t {
    kFlagExec        = 0x00000001U,      //!< Executable (.text sections).
    kFlagConst       = 0x00000002U,      //!< Read-only (.text and .data sections).
    kFlagZero        = 0x00000004U,      //!< Zero initialized by the loader (BSS).
    kFlagInfo        = 0x00000008U,      //!< Info / comment flag.
    kFlagImplicit    = 0x80000000U       //!< Section created implicitly and can be deleted by `Target`.
  };

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint32_t getId() const noexcept { return _id; }
  inline const char* getName() const noexcept { return _name; }

  inline void _setDefaultName(char c0 = 0, char c1 = 0, char c2 = 0, char c3 = 0,
                              char c4 = 0, char c5 = 0, char c6 = 0, char c7 = 0) noexcept {
    _nameAsU32[0] = IntUtils::bytepack32_4x8(uint8_t(c0), uint8_t(c1), uint8_t(c2), uint8_t(c3));
    _nameAsU32[1] = IntUtils::bytepack32_4x8(uint8_t(c4), uint8_t(c5), uint8_t(c6), uint8_t(c7));
  }

  inline bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }

  inline uint32_t getFlags() const noexcept { return _flags; }
  inline void addFlags(uint32_t flags) noexcept { _flags |= flags; }
  inline void clearFlags(uint32_t flags) noexcept { _flags &= ~flags; }

  inline uint32_t getAlignment() const noexcept { return _alignment; }
  inline void setAlignment(uint32_t alignment) noexcept { _alignment = alignment; }

  inline size_t getPhysicalSize() const noexcept { return _buffer.getLength(); }

  inline size_t getVirtualSize() const noexcept { return _virtualSize; }
  inline void setVirtualSize(uint32_t size) noexcept { _virtualSize = size; }

  inline CodeBuffer& getBuffer() noexcept { return _buffer; }
  inline const CodeBuffer& getBuffer() const noexcept { return _buffer; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _id;                          //!< Section id.
  uint32_t _flags;                       //!< Section flags.
  uint32_t _alignment;                   //!< Section alignment requirements (0 if no requirements).
  uint32_t _virtualSize;                 //!< Virtual size of the section (zero initialized mostly).
  union {
    char _name[36];                      //!< Section name (max 35 characters, PE allows max 8).
    uint32_t _nameAsU32[36 / 4];         //!< Section name as `uint32_t[]` (only optimization).
  };
  CodeBuffer _buffer;                    //!< Code or data buffer.
};

// ============================================================================
// [asmjit::LabelLink]
// ============================================================================

//! Data structure used to link labels.
struct LabelLink {
  LabelLink* prev;                       //!< Previous link (single-linked list).
  uint32_t sectionId;                    //!< Section id.
  uint32_t relocId;                      //!< Relocation id or RelocEntry::kInvalidId.
  size_t offset;                         //!< Label offset relative to the start of the section.
  intptr_t rel;                          //!< Inlined rel8/rel32.
};

// ============================================================================
// [asmjit::LabelEntry]
// ============================================================================

//! Label entry.
//!
//! Contains the following properties:
//!   * Label id - This is the only thing that is set to the `Label` operand.
//!   * Label name - Optional, used mostly to create executables and libraries.
//!   * Label type - Type of the label, default `Label::kTypeAnonymous`.
//!   * Label parent id - Derived from many assemblers that allow to define a
//!       local label that falls under a global label. This allows to define
//!       many labels of the same name that have different parent (global) label.
//!   * Offset - offset of the label bound by `Assembler`.
//!   * Links - single-linked list that contains locations of code that has
//!       to be patched when the label gets bound. Every use of unbound label
//!       adds one link to `_links` list.
//!   * HVal - Hash value of label's name and optionally parentId.
//!   * HashNext - Hash-table implementation detail.
class LabelEntry : public ZoneHashNode {
public:
  // NOTE: Label id is stored in `_customData`, which is provided by ZoneHashNode
  // to fill a padding that a C++ compiler targeting 64-bit CPU will add to align
  // the structure to 64-bits.

  //! Get label id.
  inline uint32_t getId() const noexcept { return _customData; }
  //! Set label id (internal, used only by \ref CodeHolder).
  inline void _setId(uint32_t id) noexcept { _customData = id; }

  //! Get label type, see \ref Label::Type.
  inline uint32_t getType() const noexcept { return _type; }
  //! Get label flags, returns 0 at the moment.
  inline uint32_t getFlags() const noexcept { return _flags; }

  inline bool hasParent() const noexcept { return _parentId != 0; }
  //! Get label's parent id.
  inline uint32_t getParentId() const noexcept { return _parentId; }
  //! Get label's section id where it's bound to (or `SectionEntry::kInvalidId` if it's not bound yet).
  inline uint32_t getSectionId() const noexcept { return _sectionId; }

  //! Get whether the label has name.
  inline bool hasName() const noexcept { return !_name.isEmpty(); }

  //! Get the label's name.
  //!
  //! NOTE: Local labels will return their local name without their parent
  //! part, for example ".L1".
  inline const char* getName() const noexcept { return _name.getData(); }

  //! Get length of label's name.
  //!
  //! NOTE: Label name is always null terminated, so you can use `strlen()` to
  //! get it, however, it's also cached in `LabelEntry`, so if you want to know
  //! the length the easiest way is to use `LabelEntry::getNameLength()`.
  inline uint32_t getNameLength() const noexcept { return _name.getLength(); }

  //! Get whether the label is bound.
  inline bool isBound() const noexcept { return _sectionId != SectionEntry::kInvalidId; }
  //! Get the label offset (only useful if the label is bound).
  inline intptr_t getOffset() const noexcept { return _offset; }

  //! Get the hash-value of label's name and its parent label (if any).
  //!
  //! Label hash is calculated as `HASH(Name) ^ ParentId`. The hash function
  //! is implemented in `hashString::hashString()` and `StringUtils::hashRound()`.
  inline uint32_t getHVal() const noexcept { return _hVal; }

  // ------------------------------------------------------------------------
  // [Members]
  // ------------------------------------------------------------------------

  // Let's round the size of `LabelEntry` to 64 bytes (as `ZoneAllocator` has
  // granularity of 32 bytes anyway). This gives `_name` the remaining space,
  // which is roughly 16 bytes on 64-bit and 28 bytes on 32-bit architectures.
  static constexpr uint32_t kStaticNameLength =
    64 - (sizeof(ZoneHashNode) + 16 + sizeof(intptr_t) + sizeof(LabelLink*));

  uint8_t _type;                         //!< Label type, see Label::Type.
  uint8_t _flags;                        //!< Must be zero.
  uint16_t _reserved16;                  //!< Reserved.
  uint32_t _parentId;                    //!< Label parent id or zero.
  uint32_t _sectionId;                   //!< Section id or `SectionEntry::kInvalidId`.
  uint32_t _reserved32;                  //!< Reserved.
  intptr_t _offset;                      //!< Label offset.
  LabelLink* _links;                     //!< Label links.
  SmallString<kStaticNameLength> _name;  //!< Label name.
};

// ============================================================================
// [asmjit::RelocEntry]
// ============================================================================

//! Relocation entry.
struct RelocEntry {
  enum Id : uint32_t {
    kInvalidId       = 0xFFFFFFFFU       //!< Invalid relocation id.
  };

  //! Relocation type.
  enum RelocType : uint32_t {
    kTypeNone        = 0,                //!< Deleted entry (no relocation).
    kTypeAbsToAbs    = 1,                //!< Relocate absolute to absolute.
    kTypeRelToAbs    = 2,                //!< Relocate relative to absolute.
    kTypeAbsToRel    = 3,                //!< Relocate absolute to relative.
    kTypeTrampoline  = 4                 //!< Relocate absolute to relative or use trampoline.
  };

  // ------------------------------------------------------------------------
  // [Accessors]
  // ------------------------------------------------------------------------

  inline uint32_t getId() const noexcept { return _id; }

  inline uint32_t getType() const noexcept { return _type; }
  inline uint32_t getSize() const noexcept { return _size; }

  inline uint32_t getSourceSectionId() const noexcept { return _sourceSectionId; }
  inline uint32_t getTargetSectionId() const noexcept { return _targetSectionId; }

  inline uint64_t getSourceOffset() const noexcept { return _sourceOffset; }
  inline uint64_t getData() const noexcept { return _data; }

  // ------------------------------------------------------------------------
  // [Members]
  // ------------------------------------------------------------------------

  uint32_t _id;                          //!< Relocation id.
  uint8_t _type;                         //!< Type of the relocation.
  uint8_t _size;                         //!< Size of the relocation (1, 2, 4 or 8 bytes).
  uint8_t _reserved[2];                  //!< Reserved.
  uint32_t _sourceSectionId;             //!< Source section id.
  uint32_t _targetSectionId;             //!< Destination section id.
  uint64_t _sourceOffset;                //!< Source offset (relative to start of the section).
  uint64_t _data;                        //!< Relocation data (target offset, target address, etc).
};

// ============================================================================
// [asmjit::CodeHolder]
// ============================================================================

//! Contains basic information about the target architecture plus its settings,
//! and holds code & data (including sections, labels, and relocation information).
//! CodeHolder can store both binary and intermediate representation of assembly,
//! which can be generated by \ref Assembler and/or \ref CodeBuilder.
//!
//! NOTE: CodeHolder has ability to attach an \ref ErrorHandler, however, this
//! error handler is not triggered by CodeHolder itself, it's only used by the
//! attached code generators.
class CodeHolder {
public:
  ASMJIT_NONCOPYABLE(CodeHolder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create an uninitialized CodeHolder (you must init() it before it can be used).
  ASMJIT_API CodeHolder() noexcept;
  //! Destroy the CodeHolder.
  ASMJIT_API ~CodeHolder() noexcept;

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  inline bool isInitialized() const noexcept { return _codeInfo.isInitialized(); }

  //! Initialize to CodeHolder to hold code described by `codeInfo`.
  ASMJIT_API Error init(const CodeInfo& info) noexcept;
  //! Detach all code-generators attached and reset the \ref CodeHolder.
  ASMJIT_API void reset(bool releaseMemory = false) noexcept;

  // --------------------------------------------------------------------------
  // [Attach / Detach]
  // --------------------------------------------------------------------------

  //! Attach a \ref CodeEmitter to this \ref CodeHolder.
  ASMJIT_API Error attach(CodeEmitter* emitter) noexcept;
  //! Detach a \ref CodeEmitter from this \ref CodeHolder.
  ASMJIT_API Error detach(CodeEmitter* emitter) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline ZoneAllocator* getAllocator() const noexcept { return const_cast<ZoneAllocator*>(&_allocator); }
  inline const ZoneVector<CodeEmitter*>& getEmitters() const noexcept { return _emitters; }

  // --------------------------------------------------------------------------
  // [Code / Arch]
  // --------------------------------------------------------------------------

  //! Get code/target information, see \ref CodeInfo.
  inline const CodeInfo& getCodeInfo() const noexcept { return _codeInfo; }
  //! Get architecture information, see \ref ArchInfo.
  inline const ArchInfo& getArchInfo() const noexcept { return _codeInfo.getArchInfo(); }

  //! Get the target's architecture type.
  inline uint32_t getArchType() const noexcept { return getArchInfo().getType(); }
  //! Get the target's architecture sub-type.
  inline uint32_t getArchSubType() const noexcept { return getArchInfo().getSubType(); }

  //! Get whether a static base-address is set.
  inline bool hasBaseAddress() const noexcept { return _codeInfo.hasBaseAddress(); }
  //! Get a static base-address (uint64_t).
  inline uint64_t getBaseAddress() const noexcept { return _codeInfo.getBaseAddress(); }

  // --------------------------------------------------------------------------
  // [Emitter Options]
  // --------------------------------------------------------------------------

  //! Get global hints, internally propagated to all code emitters attached.
  inline uint32_t getEmitterOptions() const noexcept { return _emitterOptions; }

  // --------------------------------------------------------------------------
  // [Result Information]
  // --------------------------------------------------------------------------

  //! Get the size code & data of all sections.
  ASMJIT_API size_t getCodeSize() const noexcept;

  //! Get size of all possible trampolines.
  //!
  //! Trampolines are needed to successfully generate relative jumps to absolute
  //! addresses. This value is only non-zero if jmp of call instructions were
  //! used with immediate operand (this means jumping or calling an absolute
  //! address directly).
  inline size_t getTrampolinesSize() const noexcept { return _trampolinesSize; }

  // --------------------------------------------------------------------------
  // [Logging & Error Handling]
  // --------------------------------------------------------------------------

  //! Get whether a logger attached.
  inline bool hasLogger() const noexcept { return _logger != nullptr; }
  //! Get the attached logger.
  inline Logger* getLogger() const noexcept { return _logger; }
  //! Attach a `logger` to CodeHolder and propagate it to all attached code emitters.
  ASMJIT_API void setLogger(Logger* logger) noexcept;
  //! Reset the logger (does nothing if not attached).
  inline void resetLogger() noexcept { setLogger(nullptr); }

  //! Get whether the global error handler is attached.
  inline bool hasErrorHandler() const noexcept { return _errorHandler != nullptr; }
  //! Get the global error handler.
  inline ErrorHandler* getErrorHandler() const noexcept { return _errorHandler; }
  //! Set the global error handler.
  inline void setErrorHandler(ErrorHandler* handler) noexcept { _errorHandler = handler; }
  //! Reset the global error handler (does nothing if not attached).
  inline void resetErrorHandler() noexcept { setErrorHandler(nullptr); }

  // --------------------------------------------------------------------------
  // [Sections]
  // --------------------------------------------------------------------------

  //! Get array of `SectionEntry*` records.
  inline const ZoneVector<SectionEntry*>& getSections() const noexcept { return _sections; }
  //! Get the number of sections.
  inline uint32_t getSectionCount() const noexcept { return _sections.getLength(); }
  //! Get a section entry of the given index.
  inline SectionEntry* getSectionEntry(uint32_t index) const noexcept { return _sections[index]; }

  ASMJIT_API Error growBuffer(CodeBuffer* cb, size_t n) noexcept;
  ASMJIT_API Error reserveBuffer(CodeBuffer* cb, size_t n) noexcept;

  // --------------------------------------------------------------------------
  // [Labels & Symbols]
  // --------------------------------------------------------------------------

  //! Create a new anonymous label and return its id in `idOut`.
  //!
  //! Returns `Error`, does not report error to \ref ErrorHandler.
  ASMJIT_API Error newLabelId(uint32_t& idOut) noexcept;

  //! Create a new named label label-type `type`.
  //!
  //! Returns `Error`, does not report error to \ref ErrorHandler.
  ASMJIT_API Error newNamedLabelId(uint32_t& idOut, const char* name, size_t nameLength, uint32_t type, uint32_t parentId) noexcept;

  //! Get a label id by name.
  ASMJIT_API uint32_t getLabelIdByName(const char* name, size_t nameLength = Globals::kNullTerminated, uint32_t parentId = 0) noexcept;

  //! Create a new label-link used to store information about yet unbound labels.
  //!
  //! Returns `null` if the allocation failed.
  ASMJIT_API LabelLink* newLabelLink(LabelEntry* le, uint32_t sectionId, size_t offset, intptr_t rel) noexcept;

  //! Get array of `LabelEntry*` records.
  inline const ZoneVector<LabelEntry*>& getLabelEntries() const noexcept { return _labelEntries; }

  //! Get number of labels created.
  inline uint32_t getLabelCount() const noexcept { return _labelEntries.getLength(); }

  //! Get number of label references, which are unresolved at the moment.
  inline uint32_t getUnresolvedLabelCount() const noexcept { return _unresolvedLabelCount; }

  //! Get whether the `label` is valid (i.e. created by `newLabelId()`).
  inline bool isLabelValid(const Label& label) const noexcept {
    return isLabelValid(label.getId());
  }
  //! Get whether the label having `id` is valid (i.e. created by `newLabelId()`).
  inline bool isLabelValid(uint32_t labelId) const noexcept {
    uint32_t index = Operand::unpackId(labelId);
    return index < _labelEntries.getLength();
  }

  //! Get whether the `label` is already bound.
  //!
  //! Returns `false` if the `label` is not valid.
  inline bool isLabelBound(const Label& label) const noexcept {
    return isLabelBound(label.getId());
  }
  //! \overload
  inline bool isLabelBound(uint32_t id) const noexcept {
    uint32_t index = Operand::unpackId(id);
    return index < _labelEntries.getLength() && _labelEntries[index]->isBound();
  }

  //! Get a `label` offset or -1 if the label is not yet bound.
  inline intptr_t getLabelOffset(const Label& label) const noexcept {
    return getLabelOffset(label.getId());
  }
  //! \overload
  inline intptr_t getLabelOffset(uint32_t id) const noexcept {
    ASMJIT_ASSERT(isLabelValid(id));
    return _labelEntries[Operand::unpackId(id)]->getOffset();
  }

  //! Get information about the given `label`.
  inline LabelEntry* getLabelEntry(const Label& label) const noexcept {
    return getLabelEntry(label.getId());
  }
  //! Get information about a label having the given `id`.
  inline LabelEntry* getLabelEntry(uint32_t id) const noexcept {
    uint32_t index = Operand::unpackId(id);
    return index < _labelEntries.getLength() ? _labelEntries[index] : static_cast<LabelEntry*>(nullptr);
  }

  // --------------------------------------------------------------------------
  // [Relocations]
  // --------------------------------------------------------------------------

  //! Get whether the code contains relocation entries.
  inline bool hasRelocEntries() const noexcept { return !_relocations.isEmpty(); }
  //! Get array of `RelocEntry*` records.
  inline const ZoneVector<RelocEntry*>& getRelocEntries() const noexcept { return _relocations; }

  //! Get reloc entry of a given `id`.
  inline RelocEntry* getRelocEntry(uint32_t id) const noexcept { return _relocations[id]; }

  //! Create a new relocation entry of type `type` and size `size`.
  //!
  //! Additional fields can be set after the relocation entry was created.
  ASMJIT_API Error newRelocEntry(RelocEntry** dst, uint32_t type, uint32_t size) noexcept;

  //! Relocate the code to `baseAddress` and copy it to `dst`.
  //!
  //! \param dst Contains the location where the relocated code should be
  //! copied. The pointer can be address returned by virtual memory allocator
  //! or any other address that has sufficient space.
  //!
  //! \param baseAddress Base address used for relocation. `JitRuntime` always
  //! sets the `baseAddress` to be the same as `dst`.
  //!
  //! \return The number of bytes actually used. If the code emitter reserved
  //! space for possible trampolines, but didn't use it, the number of bytes
  //! used can actually be less than the expected worst case. Virtual memory
  //! allocator can shrink the memory it allocated initially.
  //!
  //! A given buffer will be overwritten, to get the number of bytes required,
  //! use `getCodeSize()`.
  ASMJIT_API size_t relocate(void* dst, uint64_t baseAddress = Globals::kNoBaseAddress) const noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  CodeInfo _codeInfo;                    //!< Basic information about the code (architecture and other info).
  uint32_t _emitterOptions;              //!< Emitter options, propagated to all emitters when changed.

  Logger* _logger;                       //!< Attached \ref Logger, used by all consumers.
  ErrorHandler* _errorHandler;           //!< Attached \ref ErrorHandler.

  uint32_t _unresolvedLabelCount;        //!< Count of label references which were not resolved.
  uint32_t _trampolinesSize;             //!< Size of all possible trampolines.

  Zone _zone;                            //!< Code zone (used to allocate core structures).
  ZoneAllocator _allocator;              //!< Zone allocator, used to manage internal containers.

  ZoneVector<CodeEmitter*> _emitters;    //!< Attached code emitters.
  ZoneVector<SectionEntry*> _sections;   //!< Section entries.
  ZoneVector<LabelEntry*> _labelEntries; //!< Label entries.
  ZoneVector<RelocEntry*> _relocations;  //!< Relocation entries.
  ZoneHash<LabelEntry> _namedLabels;     //!< Label name -> LabelEntry (only named labels).
};

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_CODEHOLDER_H