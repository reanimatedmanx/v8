// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-sweep-utilities.h"

#include "src/common/globals.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/large-spaces.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/new-spaces.h"
#include "src/objects/objects-inl.h"
#include "src/objects/string-forwarding-table-inl.h"
#include "src/objects/visitors-inl.h"

namespace v8 {
namespace internal {

// The following has to hold in order for {MarkingState::MarkBitFrom} to not
// produce invalid {kImpossibleBitPattern} in the marking bitmap by overlapping.
static_assert(Heap::kMinObjectSizeInTaggedWords >= 2);

#ifdef VERIFY_HEAP
MarkingVerifierBase::MarkingVerifierBase(Heap* heap)
    : ObjectVisitorWithCageBases(heap), heap_(heap) {}

void MarkingVerifierBase::VisitMapPointer(HeapObject object) {
  VerifyMap(object->map(cage_base()));
}

void MarkingVerifierBase::VerifyRoots() {
  heap_->IterateRootsIncludingClients(
      this, base::EnumSet<SkipRoot>{SkipRoot::kWeak, SkipRoot::kTopOfStack});
}

void MarkingVerifierBase::VerifyMarkingOnPage(const Page* page, Address start,
                                              Address end) {
  Address next_object_must_be_here_or_later = start;

  for (auto [object, size] : LiveObjectRange(page)) {
    Address current = object.address();
    if (current < start) continue;
    if (current >= end) break;
    CHECK(IsMarked(object));
    CHECK(current >= next_object_must_be_here_or_later);
    object.Iterate(cage_base(), this);
    next_object_must_be_here_or_later = current + size;
    // The object is either part of a black area of black allocation or a
    // regular black object
    CHECK(bitmap(page)->AllBitsSetInRange(
              MarkingBitmap::AddressToIndex(current),
              MarkingBitmap::LimitAddressToIndex(
                  next_object_must_be_here_or_later)) ||
          bitmap(page)->AllBitsClearInRange(
              MarkingBitmap::AddressToIndex(current) + 1,
              MarkingBitmap::LimitAddressToIndex(
                  next_object_must_be_here_or_later)));
    current = next_object_must_be_here_or_later;
  }
}

void MarkingVerifierBase::VerifyMarking(NewSpace* space) {
  if (!space) return;
  if (v8_flags.minor_ms) {
    VerifyMarking(PagedNewSpace::From(space)->paged_space());
    return;
  }
  Address end = space->top();
  // The bottom position is at the start of its page. Allows us to use
  // page->area_start() as start of range on all pages.
  CHECK_EQ(space->first_allocatable_address(),
           space->first_page()->area_start());

  PageRange range(space->first_allocatable_address(), end);
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address limit = it != range.end() ? page->area_end() : end;
    CHECK(limit == end || !page->Contains(end));
    VerifyMarkingOnPage(page, page->area_start(), limit);
  }
}

void MarkingVerifierBase::VerifyMarking(PagedSpaceBase* space) {
  for (Page* p : *space) {
    VerifyMarkingOnPage(p, p->area_start(), p->area_end());
  }
}

void MarkingVerifierBase::VerifyMarking(LargeObjectSpace* lo_space) {
  if (!lo_space) return;
  LargeObjectSpaceObjectIterator it(lo_space);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    if (IsMarked(obj)) {
      obj->Iterate(cage_base(), this);
    }
  }
}
#endif  // VERIFY_HEAP

template <ExternalStringTableCleaningMode mode>
void ExternalStringTableCleanerVisitor<mode>::VisitRootPointers(
    Root root, const char* description, FullObjectSlot start,
    FullObjectSlot end) {
  // Visit all HeapObject pointers in [start, end).
  DCHECK_EQ(static_cast<int>(root),
            static_cast<int>(Root::kExternalStringsTable));
  NonAtomicMarkingState* marking_state = heap_->non_atomic_marking_state();
  Object the_hole = ReadOnlyRoots(heap_).the_hole_value();
  for (FullObjectSlot p = start; p < end; ++p) {
    Object o = *p;
    if (!IsHeapObject(o)) continue;
    HeapObject heap_object = HeapObject::cast(o);
    // MinorMS doesn't update the young strings set and so it may contain
    // strings that are already in old space.
    if (!marking_state->IsUnmarked(heap_object)) continue;
    if ((mode == ExternalStringTableCleaningMode::kYoungOnly) &&
        !Heap::InYoungGeneration(heap_object))
      continue;
    if (IsExternalString(o)) {
      heap_->FinalizeExternalString(String::cast(o));
    } else {
      // The original external string may have been internalized.
      DCHECK(IsThinString(o));
    }
    // Set the entry to the_hole_value (as deleted).
    p.store(the_hole);
  }
}

StringForwardingTableCleanerBase::StringForwardingTableCleanerBase(Heap* heap)
    : isolate_(heap->isolate()),
      marking_state_(heap->non_atomic_marking_state()) {}

void StringForwardingTableCleanerBase::DisposeExternalResource(
    StringForwardingTable::Record* record) {
  Address resource = record->ExternalResourceAddress();
  if (resource != kNullAddress && disposed_resources_.count(resource) == 0) {
    record->DisposeExternalResource();
    disposed_resources_.insert(resource);
  }
}

bool IsCppHeapMarkingFinished(
    Heap* heap, MarkingWorklists::Local* local_marking_worklists) {
  const auto* cpp_heap = CppHeap::From(heap->cpp_heap());
  if (!cpp_heap) return true;

  return cpp_heap->IsTracingDone() && local_marking_worklists->IsWrapperEmpty();
}

#if DEBUG
void VerifyRememberedSetsAfterEvacuation(Heap* heap,
                                         GarbageCollector garbage_collector) {
  // Old-to-old slot sets must be empty after evacuation.
  bool new_space_is_empty =
      !heap->new_space() || heap->new_space()->Size() == 0;
  DCHECK_IMPLIES(garbage_collector == GarbageCollector::MARK_COMPACTOR,
                 new_space_is_empty);

  MemoryChunkIterator chunk_iterator(heap);

  while (chunk_iterator.HasNext()) {
    MemoryChunk* chunk = chunk_iterator.Next();

    // Old-to-old slot sets must be empty after evacuation.
    DCHECK_NULL((chunk->slot_set<OLD_TO_OLD, AccessMode::ATOMIC>()));
    DCHECK_NULL((chunk->typed_slot_set<OLD_TO_OLD, AccessMode::ATOMIC>()));

    if (new_space_is_empty &&
        (garbage_collector == GarbageCollector::MARK_COMPACTOR)) {
      // Old-to-new slot sets must be empty after evacuation.
      DCHECK_NULL((chunk->slot_set<OLD_TO_NEW, AccessMode::ATOMIC>()));
      DCHECK_NULL((chunk->typed_slot_set<OLD_TO_NEW, AccessMode::ATOMIC>()));
      DCHECK_NULL(
          (chunk->slot_set<OLD_TO_NEW_BACKGROUND, AccessMode::ATOMIC>()));
      DCHECK_NULL(
          (chunk->typed_slot_set<OLD_TO_NEW_BACKGROUND, AccessMode::ATOMIC>()));
    }

    // Old-to-shared slots may survive GC but there should never be any slots in
    // new or shared spaces.
    AllocationSpace id = chunk->owner_identity();
    if (id == SHARED_SPACE || id == SHARED_LO_SPACE || id == NEW_SPACE ||
        id == NEW_LO_SPACE) {
      DCHECK_NULL((chunk->slot_set<OLD_TO_SHARED, AccessMode::ATOMIC>()));
      DCHECK_NULL((chunk->typed_slot_set<OLD_TO_SHARED, AccessMode::ATOMIC>()));
    }
  }
}
#endif  // DEBUG

}  // namespace internal
}  // namespace v8
