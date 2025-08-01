/*
 * Copyright (c) 2015, 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "code/codeCache.hpp"
#include "code/compiledIC.hpp"
#include "compiler/compileBroker.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "jvmci/jvmciCodeInstaller.hpp"
#include "jvmci/jvmciCompilerToVM.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "jvmci/vmStructs_jvmci.hpp"
#include "oops/klassVtable.hpp"
#include "oops/methodCounters.hpp"
#include "oops/objArrayKlass.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/continuationEntry.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/flags/jvmFlag.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/osThread.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/vm_version.hpp"
#if INCLUDE_G1GC
#include "gc/g1/g1BarrierSetRuntime.hpp"
#include "gc/g1/g1CardTable.hpp"
#include "gc/g1/g1HeapRegion.hpp"
#include "gc/g1/g1ThreadLocalData.hpp"
#endif
#if INCLUDE_ZGC
#include "gc/z/zBarrierSetAssembler.hpp"
#include "gc/z/zBarrierSetRuntime.hpp"
#include "gc/z/zThreadLocalData.hpp"
#endif
#if INCLUDE_SHENANDOAHGC
#include "gc/shenandoah/shenandoahHeap.hpp"
#include "gc/shenandoah/shenandoahRuntime.hpp"
#include "gc/shenandoah/shenandoahThreadLocalData.hpp"
#endif

#define VM_STRUCTS(nonstatic_field, static_field, unchecked_nonstatic_field, volatile_nonstatic_field) \
  static_field(CompilerToVM::Data,             oopDesc_klass_offset_in_bytes,          int)                                          \
  static_field(CompilerToVM::Data,             arrayOopDesc_length_offset_in_bytes,    int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             Klass_vtable_start_offset,              int)                                          \
  static_field(CompilerToVM::Data,             Klass_vtable_length_offset,             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             Method_extra_stack_entries,             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             SharedRuntime_ic_miss_stub,             address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_handle_wrong_method_stub, address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_unpack,        address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_unpack_with_exception_in_tls,                                \
                                                                                       address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_uncommon_trap, address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_polling_page_return_handler,                                            \
                                                                                       address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_throw_delayed_StackOverflowError_entry,                                 \
                                                                                       address)                                      \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             nmethod_entry_barrier, address)                                                       \
  static_field(CompilerToVM::Data,             thread_disarmed_guard_value_offset, int)                                              \
  static_field(CompilerToVM::Data,             thread_address_bad_mask_offset, int)                                                  \
  AARCH64_ONLY(static_field(CompilerToVM::Data, BarrierSetAssembler_nmethod_patching_type, int))                                     \
  AARCH64_ONLY(static_field(CompilerToVM::Data, BarrierSetAssembler_patching_epoch_addr, address))                                   \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_load_barrier_on_oop_field_preloaded, address)                      \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_load_barrier_on_weak_oop_field_preloaded, address)                 \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_load_barrier_on_phantom_oop_field_preloaded, address)              \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_weak_load_barrier_on_oop_field_preloaded, address)                 \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_weak_load_barrier_on_weak_oop_field_preloaded, address)            \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_weak_load_barrier_on_phantom_oop_field_preloaded, address)         \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_load_barrier_on_oop_array, address)                                \
  static_field(CompilerToVM::Data,             ZBarrierSetRuntime_clone, address)                                                    \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             ZPointerVectorLoadBadMask_address, address)                                           \
  static_field(CompilerToVM::Data,             ZPointerVectorStoreBadMask_address, address)                                          \
  static_field(CompilerToVM::Data,             ZPointerVectorStoreGoodMask_address, address)                                         \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             continuations_enabled, bool)                                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             ThreadLocalAllocBuffer_alignment_reserve, size_t)                                     \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             Universe_collectedHeap,                 CollectedHeap*)                               \
  static_field(CompilerToVM::Data,             Universe_base_vtable_size,              int)                                          \
  static_field(CompilerToVM::Data,             Universe_narrow_oop_base,               address)                                      \
  static_field(CompilerToVM::Data,             Universe_narrow_oop_shift,              int)                                          \
  static_field(CompilerToVM::Data,             Universe_narrow_klass_base,             address)                                      \
  static_field(CompilerToVM::Data,             Universe_narrow_klass_shift,            int)                                          \
  static_field(CompilerToVM::Data,             Universe_non_oop_bits,                  void*)                                        \
  static_field(CompilerToVM::Data,             Universe_verify_oop_mask,               uintptr_t)                                    \
  static_field(CompilerToVM::Data,             Universe_verify_oop_bits,               uintptr_t)                                    \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             _supports_inline_contig_alloc,          bool)                                         \
  static_field(CompilerToVM::Data,             _heap_end_addr,                         HeapWord**)                                   \
  static_field(CompilerToVM::Data,             _heap_top_addr,                         HeapWord* volatile*)                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             _max_oop_map_stack_offset,              int)                                          \
  static_field(CompilerToVM::Data,             _fields_annotations_base_offset,        int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             cardtable_start_address,                CardTable::CardValue*)                        \
  static_field(CompilerToVM::Data,             cardtable_shift,                        int)                                          \
                                                                                                                                     \
  X86_ONLY(static_field(CompilerToVM::Data,    L1_line_size,                           int))                                         \
  X86_ONLY(static_field(CompilerToVM::Data,    supports_avx512_simd_sort,              bool))                                        \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             vm_page_size,                           size_t)                                       \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             sizeof_vtableEntry,                     int)                                          \
  static_field(CompilerToVM::Data,             sizeof_ExceptionTableElement,           int)                                          \
  static_field(CompilerToVM::Data,             sizeof_LocalVariableTableElement,       int)                                          \
  static_field(CompilerToVM::Data,             sizeof_ConstantPool,                    int)                                          \
  static_field(CompilerToVM::Data,             sizeof_narrowKlass,                     int)                                          \
  static_field(CompilerToVM::Data,             sizeof_arrayOopDesc,                    int)                                          \
  static_field(CompilerToVM::Data,             sizeof_BasicLock,                       int)                                          \
  ZGC_ONLY(static_field(CompilerToVM::Data,    sizeof_ZStoreBarrierEntry,              int))                                         \
  SHENANDOAHGC_ONLY(static_field(CompilerToVM::Data, shenandoah_in_cset_fast_test_addr, address))                                    \
  SHENANDOAHGC_ONLY(static_field(CompilerToVM::Data, shenandoah_region_size_bytes_shift,int))                                        \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             dsin,                                   address)                                      \
  static_field(CompilerToVM::Data,             dcos,                                   address)                                      \
  static_field(CompilerToVM::Data,             dtan,                                   address)                                      \
  static_field(CompilerToVM::Data,             dtanh,                                  address)                                      \
  static_field(CompilerToVM::Data,             dcbrt,                                  address)                                      \
  static_field(CompilerToVM::Data,             dexp,                                   address)                                      \
  static_field(CompilerToVM::Data,             dlog,                                   address)                                      \
  static_field(CompilerToVM::Data,             dlog10,                                 address)                                      \
  static_field(CompilerToVM::Data,             dpow,                                   address)                                      \
  static_field(CompilerToVM::Data,             crc_table_addr,                         address)                                      \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             symbol_init,                            address)                                      \
  static_field(CompilerToVM::Data,             symbol_clinit,                          address)                                      \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             data_section_item_alignment,            int)                                          \
                                                                                                                                     \
  JVMTI_ONLY(static_field(CompilerToVM::Data,  _should_notify_object_alloc,            int*))                                        \
                                                                                                                                     \
  static_field(Abstract_VM_Version,            _features,                              uint64_t)                                     \
                                                                                                                                     \
  nonstatic_field(Annotations,                 _class_annotations,                     AnnotationArray*)                             \
  nonstatic_field(Annotations,                 _fields_annotations,                    Array<AnnotationArray*>*)                     \
                                                                                                                                     \
  nonstatic_field(Array<int>,                  _length,                                int)                                          \
  unchecked_nonstatic_field(Array<u1>,         _data,                                  sizeof(u1))                                   \
  unchecked_nonstatic_field(Array<u2>,         _data,                                  sizeof(u2))                                   \
  nonstatic_field(Array<Klass*>,               _length,                                int)                                          \
  nonstatic_field(Array<Klass*>,               _data[0],                               Klass*)                                       \
                                                                                                                                     \
  volatile_nonstatic_field(BasicLock,          _metadata,                              uintptr_t)                                    \
                                                                                                                                     \
  static_field(CodeCache,                      _low_bound,                             address)                                      \
  static_field(CodeCache,                      _high_bound,                            address)                                      \
                                                                                                                                     \
  nonstatic_field(CollectedHeap,               _total_collections,                     unsigned int)                                 \
                                                                                                                                     \
  nonstatic_field(CompileTask,                 _num_inlined_bytecodes,                 int)                                          \
                                                                                                                                     \
  volatile_nonstatic_field(CompiledICData,     _speculated_method,                     Method*)                                      \
  volatile_nonstatic_field(CompiledICData,     _speculated_klass,                      uintptr_t)                                    \
  nonstatic_field(CompiledICData,              _itable_defc_klass,                     Klass*)                                       \
  nonstatic_field(CompiledICData,              _itable_refc_klass,                     Klass*)                                       \
                                                                                                                                     \
  nonstatic_field(ConstantPool,                _tags,                                  Array<u1>*)                                   \
  nonstatic_field(ConstantPool,                _pool_holder,                           InstanceKlass*)                               \
  nonstatic_field(ConstantPool,                _length,                                int)                                          \
  nonstatic_field(ConstantPool,                _flags,                                 u2)                                           \
  nonstatic_field(ConstantPool,                _source_file_name_index,                u2)                                           \
                                                                                                                                     \
  nonstatic_field(ConstMethod,                 _constants,                             ConstantPool*)                                \
  nonstatic_field(ConstMethod,                 _flags._flags,                          u4)                                           \
  nonstatic_field(ConstMethod,                 _code_size,                             u2)                                           \
  nonstatic_field(ConstMethod,                 _name_index,                            u2)                                           \
  nonstatic_field(ConstMethod,                 _signature_index,                       u2)                                           \
  nonstatic_field(ConstMethod,                 _method_idnum,                          u2)                                           \
  nonstatic_field(ConstMethod,                 _max_stack,                             u2)                                           \
  nonstatic_field(ConstMethod,                 _max_locals,                            u2)                                           \
                                                                                                                                     \
  nonstatic_field(DataLayout,                  _header._struct._tag,                   u1)                                           \
  nonstatic_field(DataLayout,                  _header._struct._flags,                 u1)                                           \
  nonstatic_field(DataLayout,                  _header._struct._bci,                   u2)                                           \
  nonstatic_field(DataLayout,                  _header._struct._traps,                 u4)                                           \
  nonstatic_field(DataLayout,                  _cells[0],                              intptr_t)                                     \
                                                                                                                                     \
  nonstatic_field(Deoptimization::UnrollBlock, _size_of_deoptimized_frame,             int)                                          \
  nonstatic_field(Deoptimization::UnrollBlock, _caller_adjustment,                     int)                                          \
  nonstatic_field(Deoptimization::UnrollBlock, _number_of_frames,                      int)                                          \
  nonstatic_field(Deoptimization::UnrollBlock, _total_frame_sizes,                     int)                                          \
  nonstatic_field(Deoptimization::UnrollBlock, _frame_sizes,                           intptr_t*)                                    \
  nonstatic_field(Deoptimization::UnrollBlock, _frame_pcs,                             address*)                                     \
  nonstatic_field(Deoptimization::UnrollBlock, _initial_info,                          intptr_t)                                     \
  nonstatic_field(Deoptimization::UnrollBlock, _unpack_kind,                           int)                                          \
                                                                                                                                     \
  nonstatic_field(ExceptionTableElement,       start_pc,                                      u2)                                    \
  nonstatic_field(ExceptionTableElement,       end_pc,                                        u2)                                    \
  nonstatic_field(ExceptionTableElement,       handler_pc,                                    u2)                                    \
  nonstatic_field(ExceptionTableElement,       catch_type_index,                              u2)                                    \
                                                                                                                                     \
  nonstatic_field(InstanceKlass,               _fieldinfo_stream,                             Array<u1>*)                            \
  nonstatic_field(InstanceKlass,               _constants,                                    ConstantPool*)                         \
  volatile_nonstatic_field(InstanceKlass,      _init_state,                                   InstanceKlass::ClassState)             \
  volatile_nonstatic_field(InstanceKlass,      _init_thread,                                  JavaThread*)                           \
  nonstatic_field(InstanceKlass,               _misc_flags._flags,                            u2)                                    \
  nonstatic_field(InstanceKlass,               _annotations,                                  Annotations*)                          \
                                                                                                                                     \
  volatile_nonstatic_field(JavaFrameAnchor,    _last_Java_sp,                                 intptr_t*)                             \
  volatile_nonstatic_field(JavaFrameAnchor,    _last_Java_pc,                                 address)                               \
                                                                                                                                     \
  nonstatic_field(JVMCICompileState,           _jvmti_can_hotswap_or_post_breakpoint,         jbyte)                                 \
  nonstatic_field(JVMCICompileState,           _jvmti_can_access_local_variables,             jbyte)                                 \
  nonstatic_field(JVMCICompileState,           _jvmti_can_post_on_exceptions,                 jbyte)                                 \
  nonstatic_field(JVMCICompileState,           _jvmti_can_pop_frame,                          jbyte)                                 \
  nonstatic_field(JVMCICompileState,           _compilation_ticks,                            jint)                                  \
                                                                                                                                     \
  nonstatic_field(JavaThread,                  _threadObj,                                    OopHandle)                             \
  nonstatic_field(JavaThread,                  _vthread,                                      OopHandle)                             \
  nonstatic_field(JavaThread,                  _scopedValueCache,                             OopHandle)                             \
  nonstatic_field(JavaThread,                  _anchor,                                       JavaFrameAnchor)                       \
  nonstatic_field(JavaThread,                  _monitor_owner_id,                             int64_t)                               \
  nonstatic_field(JavaThread,                  _vm_result_oop,                                oop)                                   \
  nonstatic_field(JavaThread,                  _stack_overflow_state._stack_overflow_limit,   address)                               \
  volatile_nonstatic_field(JavaThread,         _exception_oop,                                oop)                                   \
  volatile_nonstatic_field(JavaThread,         _exception_pc,                                 address)                               \
  volatile_nonstatic_field(JavaThread,         _is_method_handle_return,                      int)                                   \
  volatile_nonstatic_field(JavaThread,         _doing_unsafe_access,                          bool)                                  \
  nonstatic_field(JavaThread,                  _osthread,                                     OSThread*)                             \
  nonstatic_field(JavaThread,                  _saved_exception_pc,                           address)                               \
  nonstatic_field(JavaThread,                  _pending_deoptimization,                       int)                                   \
  nonstatic_field(JavaThread,                  _pending_failed_speculation,                   jlong)                                 \
  nonstatic_field(JavaThread,                  _pending_transfer_to_interpreter,              bool)                                  \
  nonstatic_field(JavaThread,                  _jvmci_counters,                               jlong*)                                \
  nonstatic_field(JavaThread,                  _jvmci_reserved0,                              jlong)                                 \
  nonstatic_field(JavaThread,                  _jvmci_reserved1,                              jlong)                                 \
  nonstatic_field(JavaThread,                  _jvmci_reserved_oop0,                          oop)                                   \
  nonstatic_field(JavaThread,                  _should_post_on_exceptions_flag,               int)                                   \
  nonstatic_field(JavaThread,                  _jni_environment,                              JNIEnv)                                \
  nonstatic_field(JavaThread,                  _stack_overflow_state._reserved_stack_activation, address)                            \
  nonstatic_field(JavaThread,                  _held_monitor_count,                           intx)                                  \
  nonstatic_field(JavaThread,                  _lock_stack,                                   LockStack)                             \
  nonstatic_field(JavaThread,                  _om_cache,                                     OMCache)                               \
  nonstatic_field(JavaThread,                  _cont_entry,                                   ContinuationEntry*)                    \
  nonstatic_field(JavaThread,                  _unlocked_inflated_monitor,                    ObjectMonitor*)                        \
  JVMTI_ONLY(nonstatic_field(JavaThread,       _is_in_VTMS_transition,                        bool))                                 \
  JVMTI_ONLY(nonstatic_field(JavaThread,       _is_disable_suspend,                           bool))                                 \
                                                                                                                                     \
  nonstatic_field(ContinuationEntry,           _pin_count,                                    uint32_t)                              \
  nonstatic_field(LockStack,                   _top,                                          uint32_t)                              \
                                                                                                                                     \
  JVMTI_ONLY(static_field(JvmtiVTMSTransitionDisabler, _VTMS_notify_jvmti_events,             bool))                                 \
                                                                                                                                     \
  static_field(java_lang_Class,                _klass_offset,                                 int)                                   \
  static_field(java_lang_Class,                _array_klass_offset,                           int)                                   \
                                                                                                                                     \
  nonstatic_field(InvocationCounter,           _counter,                                      unsigned int)                          \
                                                                                                                                     \
  nonstatic_field(Klass,                       _secondary_super_cache,                        Klass*)                                \
  nonstatic_field(Klass,                       _secondary_supers,                             Array<Klass*>*)                        \
  nonstatic_field(Klass,                       _super,                                        Klass*)                                \
  nonstatic_field(Klass,                       _super_check_offset,                           juint)                                 \
  volatile_nonstatic_field(Klass,              _subklass,                                     Klass*)                                \
  nonstatic_field(Klass,                       _layout_helper,                                jint)                                  \
  nonstatic_field(Klass,                       _name,                                         Symbol*)                               \
  volatile_nonstatic_field(Klass,              _next_sibling,                                 Klass*)                                \
  nonstatic_field(Klass,                       _java_mirror,                                  OopHandle)                             \
  nonstatic_field(Klass,                       _access_flags,                                 AccessFlags)                           \
  nonstatic_field(Klass,                       _class_loader_data,                            ClassLoaderData*)                      \
  nonstatic_field(Klass,                       _secondary_supers_bitmap,                      uintx)                                 \
  nonstatic_field(Klass,                       _hash_slot,                                    uint8_t)                               \
  nonstatic_field(Klass,                       _misc_flags._flags,                            u1)                                    \
  nonstatic_field(Klass,                       _prototype_header,                             markWord)                              \
                                                                                                                                     \
  nonstatic_field(LocalVariableTableElement,   start_bci,                                     u2)                                    \
  nonstatic_field(LocalVariableTableElement,   length,                                        u2)                                    \
  nonstatic_field(LocalVariableTableElement,   name_cp_index,                                 u2)                                    \
  nonstatic_field(LocalVariableTableElement,   descriptor_cp_index,                           u2)                                    \
  nonstatic_field(LocalVariableTableElement,   signature_cp_index,                            u2)                                    \
  nonstatic_field(LocalVariableTableElement,   slot,                                          u2)                                    \
                                                                                                                                     \
  nonstatic_field(Method,                      _constMethod,                                  ConstMethod*)                          \
  nonstatic_field(Method,                      _method_data,                                  MethodData*)                           \
  nonstatic_field(Method,                      _method_counters,                              MethodCounters*)                       \
  nonstatic_field(Method,                      _access_flags,                                 AccessFlags)                           \
  nonstatic_field(Method,                      _vtable_index,                                 int)                                   \
  nonstatic_field(Method,                      _intrinsic_id,                                 u2)                                    \
  nonstatic_field(Method,                      _flags._status,                                u4)                                    \
  volatile_nonstatic_field(Method,             _code,                                         nmethod*)                              \
  volatile_nonstatic_field(Method,             _from_compiled_entry,                          address)                               \
                                                                                                                                     \
  nonstatic_field(MethodCounters,              _invoke_mask,                                  int)                                   \
  nonstatic_field(MethodCounters,              _backedge_mask,                                int)                                   \
  nonstatic_field(MethodCounters,              _interpreter_throwout_count,                   u2)                                    \
  JVMTI_ONLY(nonstatic_field(MethodCounters,   _number_of_breakpoints,                        u2))                                   \
  nonstatic_field(MethodCounters,              _invocation_counter,                           InvocationCounter)                     \
  nonstatic_field(MethodCounters,              _backedge_counter,                             InvocationCounter)                     \
                                                                                                                                     \
  nonstatic_field(MethodData,                  _size,                                         int)                                   \
  nonstatic_field(MethodData,                  _method,                                       Method*)                               \
  nonstatic_field(MethodData,                  _data_size,                                    int)                                   \
  nonstatic_field(MethodData,                  _data[0],                                      intptr_t)                              \
  nonstatic_field(MethodData,                  _parameters_type_data_di,                      int)                                   \
  nonstatic_field(MethodData,                  _compiler_counters._nof_decompiles,            uint)                                  \
  nonstatic_field(MethodData,                  _compiler_counters._nof_overflow_recompiles,   uint)                                  \
  nonstatic_field(MethodData,                  _compiler_counters._nof_overflow_traps,        uint)                                  \
  nonstatic_field(MethodData,                  _compiler_counters._trap_hist._array[0],       u1)                                    \
  nonstatic_field(MethodData,                  _eflags,                                       intx)                                  \
  nonstatic_field(MethodData,                  _arg_local,                                    intx)                                  \
  nonstatic_field(MethodData,                  _arg_stack,                                    intx)                                  \
  nonstatic_field(MethodData,                  _arg_returned,                                 intx)                                  \
  nonstatic_field(MethodData,                  _tenure_traps,                                 uint)                                  \
  nonstatic_field(MethodData,                  _invoke_mask,                                  int)                                   \
  nonstatic_field(MethodData,                  _backedge_mask,                                int)                                   \
  nonstatic_field(MethodData,                  _jvmci_ir_size,                                int)                                   \
                                                                                                                                     \
  nonstatic_field(nmethod,                     _verified_entry_offset,                        u2)                                    \
  nonstatic_field(nmethod,                     _comp_level,                                   CompLevel)                             \
                                                                                                                                     \
  nonstatic_field(ObjArrayKlass,               _element_klass,                                Klass*)                                \
                                                                                                                                     \
  volatile_nonstatic_field(ObjectMonitor,      _owner,                                        int64_t)                               \
  volatile_nonstatic_field(ObjectMonitor,      _recursions,                                   intptr_t)                              \
  volatile_nonstatic_field(ObjectMonitor,      _entry_list,                                   ObjectWaiter*)                         \
  volatile_nonstatic_field(ObjectMonitor,      _succ,                                         int64_t)                               \
  volatile_nonstatic_field(ObjectMonitor,      _stack_locker,                                 BasicLock*)                            \
                                                                                                                                     \
  volatile_nonstatic_field(oopDesc,            _mark,                                         markWord)                              \
  volatile_nonstatic_field(oopDesc,            _metadata._klass,                              Klass*)                                \
                                                                                                                                     \
  static_field(StubRoutines,                _verify_oop_count,                                jint)                                  \
                                                                                                                                     \
  static_field(StubRoutines,                _jbyte_arraycopy,                                 address)                               \
  static_field(StubRoutines,                _jshort_arraycopy,                                address)                               \
  static_field(StubRoutines,                _jint_arraycopy,                                  address)                               \
  static_field(StubRoutines,                _jlong_arraycopy,                                 address)                               \
  static_field(StubRoutines,                _oop_arraycopy,                                   address)                               \
  static_field(StubRoutines,                _oop_arraycopy_uninit,                            address)                               \
  static_field(StubRoutines,                _jbyte_disjoint_arraycopy,                        address)                               \
  static_field(StubRoutines,                _jshort_disjoint_arraycopy,                       address)                               \
  static_field(StubRoutines,                _jint_disjoint_arraycopy,                         address)                               \
  static_field(StubRoutines,                _jlong_disjoint_arraycopy,                        address)                               \
  static_field(StubRoutines,                _oop_disjoint_arraycopy,                          address)                               \
  static_field(StubRoutines,                _oop_disjoint_arraycopy_uninit,                   address)                               \
  static_field(StubRoutines,                _arrayof_jbyte_arraycopy,                         address)                               \
  static_field(StubRoutines,                _arrayof_jshort_arraycopy,                        address)                               \
  static_field(StubRoutines,                _arrayof_jint_arraycopy,                          address)                               \
  static_field(StubRoutines,                _arrayof_jlong_arraycopy,                         address)                               \
  static_field(StubRoutines,                _arrayof_oop_arraycopy,                           address)                               \
  static_field(StubRoutines,                _arrayof_oop_arraycopy_uninit,                    address)                               \
  static_field(StubRoutines,                _arrayof_jbyte_disjoint_arraycopy,                address)                               \
  static_field(StubRoutines,                _arrayof_jshort_disjoint_arraycopy,               address)                               \
  static_field(StubRoutines,                _arrayof_jint_disjoint_arraycopy,                 address)                               \
  static_field(StubRoutines,                _arrayof_jlong_disjoint_arraycopy,                address)                               \
  static_field(StubRoutines,                _arrayof_oop_disjoint_arraycopy,                  address)                               \
  static_field(StubRoutines,                _arrayof_oop_disjoint_arraycopy_uninit,           address)                               \
  static_field(StubRoutines,                _checkcast_arraycopy,                             address)                               \
  static_field(StubRoutines,                _checkcast_arraycopy_uninit,                      address)                               \
  static_field(StubRoutines,                _unsafe_arraycopy,                                address)                               \
  static_field(StubRoutines,                _generic_arraycopy,                               address)                               \
  static_field(StubRoutines,                _array_sort,                                      address)                               \
  static_field(StubRoutines,                _array_partition,                                 address)                               \
  static_field(StubRoutines,                _unsafe_setmemory,                                address)                               \
                                                                                                                                     \
  static_field(StubRoutines,                _aescrypt_encryptBlock,                           address)                               \
  static_field(StubRoutines,                _aescrypt_decryptBlock,                           address)                               \
  static_field(StubRoutines,                _cipherBlockChaining_encryptAESCrypt,             address)                               \
  static_field(StubRoutines,                _cipherBlockChaining_decryptAESCrypt,             address)                               \
  static_field(StubRoutines,                _electronicCodeBook_encryptAESCrypt,              address)                               \
  static_field(StubRoutines,                _electronicCodeBook_decryptAESCrypt,              address)                               \
  static_field(StubRoutines,                _counterMode_AESCrypt,                            address)                               \
  static_field(StubRoutines,                _galoisCounterMode_AESCrypt,                      address)                               \
  static_field(StubRoutines,                _base64_encodeBlock,                              address)                               \
  static_field(StubRoutines,                _base64_decodeBlock,                              address)                               \
  static_field(StubRoutines,                _ghash_processBlocks,                             address)                               \
  static_field(StubRoutines,                _md5_implCompress,                                address)                               \
  static_field(StubRoutines,                _md5_implCompressMB,                              address)                               \
  static_field(StubRoutines,                _chacha20Block,                                   address)                               \
  static_field(StubRoutines,                _poly1305_processBlocks,                          address)                               \
  static_field(StubRoutines,                _intpoly_montgomeryMult_P256,                     address)                               \
  static_field(StubRoutines,                _intpoly_assign,                                  address)                               \
  static_field(StubRoutines,                _sha1_implCompress,                               address)                               \
  static_field(StubRoutines,                _sha1_implCompressMB,                             address)                               \
  static_field(StubRoutines,                _sha256_implCompress,                             address)                               \
  static_field(StubRoutines,                _sha256_implCompressMB,                           address)                               \
  static_field(StubRoutines,                _sha512_implCompress,                             address)                               \
  static_field(StubRoutines,                _sha512_implCompressMB,                           address)                               \
  static_field(StubRoutines,                _sha3_implCompress,                               address)                               \
  static_field(StubRoutines,                _double_keccak,                                   address)                               \
  static_field(StubRoutines,                _sha3_implCompressMB,                             address)                               \
  static_field(StubRoutines,                _kyberNtt,                                        address)                               \
  static_field(StubRoutines,                _kyberInverseNtt,                                 address)                               \
  static_field(StubRoutines,                _kyberNttMult,                                    address)                               \
  static_field(StubRoutines,                _kyberAddPoly_2,                                  address)                               \
  static_field(StubRoutines,                _kyberAddPoly_3,                                  address)                               \
  static_field(StubRoutines,                _kyber12To16,                                     address)                               \
  static_field(StubRoutines,                _kyberBarrettReduce,                              address)                               \
  static_field(StubRoutines,                _dilithiumAlmostNtt,                              address)                               \
  static_field(StubRoutines,                _dilithiumAlmostInverseNtt,                       address)                               \
  static_field(StubRoutines,                _dilithiumNttMult,                                address)                               \
  static_field(StubRoutines,                _dilithiumMontMulByConstant,                      address)                               \
  static_field(StubRoutines,                _dilithiumDecomposePoly,                          address)                               \
  static_field(StubRoutines,                _updateBytesCRC32,                                address)                               \
  static_field(StubRoutines,                _updateBytesCRC32C,                               address)                               \
  static_field(StubRoutines,                _updateBytesAdler32,                              address)                               \
  static_field(StubRoutines,                _multiplyToLen,                                   address)                               \
  static_field(StubRoutines,                _squareToLen,                                     address)                               \
  static_field(StubRoutines,                _mulAdd,                                          address)                               \
  static_field(StubRoutines,                _montgomeryMultiply,                              address)                               \
  static_field(StubRoutines,                _montgomerySquare,                                address)                               \
  static_field(StubRoutines,                _vectorizedMismatch,                              address)                               \
  static_field(StubRoutines,                _bigIntegerRightShiftWorker,                      address)                               \
  static_field(StubRoutines,                _bigIntegerLeftShiftWorker,                       address)                               \
  static_field(StubRoutines,                _cont_thaw,                                       address)                               \
  static_field(StubRoutines,                _lookup_secondary_supers_table_slow_path_stub,    address)                               \
                                                                                                                                     \
  nonstatic_field(Thread,                   _poll_data,                                       SafepointMechanism::ThreadData)        \
  nonstatic_field(Thread,                   _tlab,                                            ThreadLocalAllocBuffer)                \
  nonstatic_field(Thread,                   _allocated_bytes,                                 jlong)                                 \
  JFR_ONLY(nonstatic_field(Thread,          _jfr_thread_local,                                JfrThreadLocal))                       \
                                                                                                                                     \
  static_field(java_lang_Thread,            _tid_offset,                                      int)                                   \
  static_field(java_lang_Thread,            _jvmti_is_in_VTMS_transition_offset,              int)                                   \
  JFR_ONLY(static_field(java_lang_Thread,   _jfr_epoch_offset,                                int))                                  \
                                                                                                                                     \
  JFR_ONLY(nonstatic_field(JfrThreadLocal,  _vthread_id,                                      traceid))                              \
  JFR_ONLY(nonstatic_field(JfrThreadLocal,  _vthread_epoch,                                   u2))                                   \
  JFR_ONLY(nonstatic_field(JfrThreadLocal,  _vthread_excluded,                                bool))                                 \
  JFR_ONLY(nonstatic_field(JfrThreadLocal,  _vthread,                                         bool))                                 \
                                                                                                                                     \
  nonstatic_field(ThreadLocalAllocBuffer,   _start,                                           HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,   _top,                                             HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,   _end,                                             HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,   _pf_top,                                          HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,   _desired_size,                                    size_t)                                \
  nonstatic_field(ThreadLocalAllocBuffer,   _refill_waste_limit,                              size_t)                                \
  nonstatic_field(ThreadLocalAllocBuffer,   _number_of_refills,                               unsigned)                              \
  nonstatic_field(ThreadLocalAllocBuffer,   _slow_allocations,                                unsigned)                              \
                                                                                                                                     \
  nonstatic_field(SafepointMechanism::ThreadData, _polling_word,                              volatile uintptr_t)                    \
  nonstatic_field(SafepointMechanism::ThreadData, _polling_page,                              volatile uintptr_t)                    \
                                                                                                                                     \
  nonstatic_field(ThreadShadow,             _pending_exception,                               oop)                                   \
                                                                                                                                     \
  static_field(Symbol,                      _vm_symbols[0],                                   Symbol*)                               \
                                                                                                                                     \
  nonstatic_field(vtableEntry,              _method,                                          Method*)                               \

#define VM_TYPES(declare_type, declare_toplevel_type, declare_integer_type, declare_unsigned_integer_type) \
  declare_integer_type(bool)                                              \
  declare_unsigned_integer_type(size_t)                                   \
  declare_integer_type(intx)                                              \
  declare_unsigned_integer_type(uintx)                                    \
  declare_integer_type(CompLevel)                                         \
                                                                          \
  declare_toplevel_type(BasicLock)                                        \
  declare_toplevel_type(CompilerToVM)                                     \
  declare_toplevel_type(ExceptionTableElement)                            \
  declare_toplevel_type(JVMFlag)                                          \
  declare_toplevel_type(JVMFlag*)                                         \
  declare_toplevel_type(InvocationCounter)                                \
  declare_toplevel_type(JVMCICompileState)                                \
  declare_toplevel_type(JVMCIEnv)                                         \
  declare_toplevel_type(LocalVariableTableElement)                        \
  declare_toplevel_type(narrowKlass)                                      \
  declare_toplevel_type(ObjectWaiter)                                     \
  declare_toplevel_type(Symbol*)                                          \
  declare_toplevel_type(vtableEntry)                                      \
                                                                          \
  declare_toplevel_type(oopDesc)                                          \
    declare_type(arrayOopDesc, oopDesc)                                   \
                                                                          \
  declare_toplevel_type(CompiledICData)                                   \
  declare_toplevel_type(MetaspaceObj)                                     \
    declare_type(Metadata, MetaspaceObj)                                  \
    declare_type(Klass, Metadata)                                         \
      declare_type(InstanceKlass, Klass)                                  \
    declare_type(ConstantPool, Metadata)                                  \

#define VM_INT_CONSTANTS(declare_constant, declare_constant_with_value, declare_preprocessor_constant) \
  declare_preprocessor_constant("ASSERT", DEBUG_ONLY(1) NOT_DEBUG(0))     \
                                                                          \
  declare_preprocessor_constant("INCLUDE_SERIALGC", INCLUDE_SERIALGC)     \
  declare_preprocessor_constant("INCLUDE_PARALLELGC", INCLUDE_PARALLELGC) \
  declare_preprocessor_constant("INCLUDE_G1GC", INCLUDE_G1GC)             \
  declare_preprocessor_constant("INCLUDE_ZGC", INCLUDE_ZGC)               \
  declare_preprocessor_constant("INCLUDE_SHENANDOAHGC", INCLUDE_SHENANDOAHGC) \
                                                                          \
  declare_constant(CompLevel_none)                                        \
  declare_constant(CompLevel_simple)                                      \
  declare_constant(CompLevel_limited_profile)                             \
  declare_constant(CompLevel_full_profile)                                \
  declare_constant(CompLevel_full_optimization)                           \
  declare_constant(HeapWordSize)                                          \
  declare_constant(InvocationEntryBci)                                    \
  declare_constant(JVMCINMethodData::SPECULATION_LENGTH_BITS)             \
                                                                          \
  declare_constant(FieldInfo::FieldFlags::_ff_injected)                   \
  declare_constant(FieldInfo::FieldFlags::_ff_stable)                     \
  declare_preprocessor_constant("JVM_ACC_VARARGS", JVM_ACC_VARARGS)       \
  declare_preprocessor_constant("JVM_ACC_BRIDGE", JVM_ACC_BRIDGE)         \
  declare_preprocessor_constant("JVM_ACC_ANNOTATION", JVM_ACC_ANNOTATION) \
  declare_preprocessor_constant("JVM_ACC_ENUM", JVM_ACC_ENUM)             \
  declare_preprocessor_constant("JVM_ACC_SYNTHETIC", JVM_ACC_SYNTHETIC)   \
  declare_preprocessor_constant("JVM_ACC_INTERFACE", JVM_ACC_INTERFACE)   \
                                                                          \
  declare_constant(JVM_CONSTANT_Utf8)                                     \
  declare_constant(JVM_CONSTANT_Unicode)                                  \
  declare_constant(JVM_CONSTANT_Integer)                                  \
  declare_constant(JVM_CONSTANT_Float)                                    \
  declare_constant(JVM_CONSTANT_Long)                                     \
  declare_constant(JVM_CONSTANT_Double)                                   \
  declare_constant(JVM_CONSTANT_Class)                                    \
  declare_constant(JVM_CONSTANT_String)                                   \
  declare_constant(JVM_CONSTANT_Fieldref)                                 \
  declare_constant(JVM_CONSTANT_Methodref)                                \
  declare_constant(JVM_CONSTANT_InterfaceMethodref)                       \
  declare_constant(JVM_CONSTANT_NameAndType)                              \
  declare_constant(JVM_CONSTANT_MethodHandle)                             \
  declare_constant(JVM_CONSTANT_MethodType)                               \
  declare_constant(JVM_CONSTANT_InvokeDynamic)                            \
  declare_constant(JVM_CONSTANT_Dynamic)                                  \
  declare_constant(JVM_CONSTANT_Module)                                   \
  declare_constant(JVM_CONSTANT_Package)                                  \
  declare_constant(JVM_CONSTANT_ExternalMax)                              \
                                                                          \
  declare_constant(JVM_CONSTANT_Invalid)                                  \
  declare_constant(JVM_CONSTANT_InternalMin)                              \
  declare_constant(JVM_CONSTANT_UnresolvedClass)                          \
  declare_constant(JVM_CONSTANT_ClassIndex)                               \
  declare_constant(JVM_CONSTANT_StringIndex)                              \
  declare_constant(JVM_CONSTANT_UnresolvedClassInError)                   \
  declare_constant(JVM_CONSTANT_MethodHandleInError)                      \
  declare_constant(JVM_CONSTANT_MethodTypeInError)                        \
  declare_constant(JVM_CONSTANT_DynamicInError)                           \
  declare_constant(JVM_CONSTANT_InternalMax)                              \
                                                                          \
  declare_constant(ArrayData::array_len_off_set)                          \
  declare_constant(ArrayData::array_start_off_set)                        \
                                                                          \
  declare_constant(BitData::exception_seen_flag)                          \
  declare_constant(BitData::null_seen_flag)                               \
  declare_constant(BranchData::not_taken_off_set)                         \
                                                                          \
  declare_constant_with_value("CardTable::dirty_card", CardTable::dirty_card_val()) \
  declare_constant_with_value("LockStack::_end_offset", LockStack::end_offset()) \
  declare_constant_with_value("OMCache::oop_to_oop_difference", OMCache::oop_to_oop_difference()) \
  declare_constant_with_value("OMCache::oop_to_monitor_difference", OMCache::oop_to_monitor_difference()) \
                                                                                          \
  declare_constant(nmethod::InvalidationReason::NOT_INVALIDATED)                          \
  declare_constant(nmethod::InvalidationReason::C1_CODEPATCH)                             \
  declare_constant(nmethod::InvalidationReason::C1_DEOPTIMIZE)                            \
  declare_constant(nmethod::InvalidationReason::C1_DEOPTIMIZE_FOR_PATCHING)               \
  declare_constant(nmethod::InvalidationReason::C1_PREDICATE_FAILED_TRAP)                 \
  declare_constant(nmethod::InvalidationReason::CI_REPLAY)                                \
  declare_constant(nmethod::InvalidationReason::UNLOADING)                                \
  declare_constant(nmethod::InvalidationReason::UNLOADING_COLD)                           \
  declare_constant(nmethod::InvalidationReason::JVMCI_INVALIDATE)                         \
  declare_constant(nmethod::InvalidationReason::JVMCI_MATERIALIZE_VIRTUAL_OBJECT)         \
  declare_constant(nmethod::InvalidationReason::JVMCI_REPLACED_WITH_NEW_CODE)             \
  declare_constant(nmethod::InvalidationReason::JVMCI_REPROFILE)                          \
  declare_constant(nmethod::InvalidationReason::MARKED_FOR_DEOPTIMIZATION)                \
  declare_constant(nmethod::InvalidationReason::MISSING_EXCEPTION_HANDLER)                \
  declare_constant(nmethod::InvalidationReason::NOT_USED)                                 \
  declare_constant(nmethod::InvalidationReason::OSR_INVALIDATION_BACK_BRANCH)             \
  declare_constant(nmethod::InvalidationReason::OSR_INVALIDATION_FOR_COMPILING_WITH_C1)   \
  declare_constant(nmethod::InvalidationReason::OSR_INVALIDATION_OF_LOWER_LEVEL)          \
  declare_constant(nmethod::InvalidationReason::SET_NATIVE_FUNCTION)                      \
  declare_constant(nmethod::InvalidationReason::UNCOMMON_TRAP)                            \
  declare_constant(nmethod::InvalidationReason::WHITEBOX_DEOPTIMIZATION)                  \
  declare_constant(nmethod::InvalidationReason::ZOMBIE)                                   \
                                                                                          \
  declare_constant(CodeInstaller::VERIFIED_ENTRY)                         \
  declare_constant(CodeInstaller::UNVERIFIED_ENTRY)                       \
  declare_constant(CodeInstaller::OSR_ENTRY)                              \
  declare_constant(CodeInstaller::EXCEPTION_HANDLER_ENTRY)                \
  declare_constant(CodeInstaller::DEOPT_HANDLER_ENTRY)                    \
  declare_constant(CodeInstaller::FRAME_COMPLETE)                         \
  declare_constant(CodeInstaller::ENTRY_BARRIER_PATCH)                    \
  declare_constant(CodeInstaller::INVOKEINTERFACE)                        \
  declare_constant(CodeInstaller::INVOKEVIRTUAL)                          \
  declare_constant(CodeInstaller::INVOKESTATIC)                           \
  declare_constant(CodeInstaller::INVOKESPECIAL)                          \
  declare_constant(CodeInstaller::INLINE_INVOKE)                          \
  declare_constant(CodeInstaller::POLL_NEAR)                              \
  declare_constant(CodeInstaller::POLL_RETURN_NEAR)                       \
  declare_constant(CodeInstaller::POLL_FAR)                               \
  declare_constant(CodeInstaller::POLL_RETURN_FAR)                        \
  declare_constant(CodeInstaller::CARD_TABLE_SHIFT)                       \
  declare_constant(CodeInstaller::CARD_TABLE_ADDRESS)                     \
  declare_constant(CodeInstaller::HEAP_TOP_ADDRESS)                       \
  declare_constant(CodeInstaller::HEAP_END_ADDRESS)                       \
  declare_constant(CodeInstaller::NARROW_KLASS_BASE_ADDRESS)              \
  declare_constant(CodeInstaller::NARROW_OOP_BASE_ADDRESS)                \
  declare_constant(CodeInstaller::CRC_TABLE_ADDRESS)                      \
  declare_constant(CodeInstaller::LOG_OF_HEAP_REGION_GRAIN_BYTES)         \
  declare_constant(CodeInstaller::INLINE_CONTIGUOUS_ALLOCATION_SUPPORTED) \
  declare_constant(CodeInstaller::DEOPT_MH_HANDLER_ENTRY)                 \
  declare_constant(CodeInstaller::VERIFY_OOP_COUNT_ADDRESS)               \
  declare_constant(CodeInstaller::VERIFY_OOPS)                            \
  declare_constant(CodeInstaller::VERIFY_OOP_BITS)                        \
  declare_constant(CodeInstaller::VERIFY_OOP_MASK)                        \
  declare_constant(CodeInstaller::INVOKE_INVALID)                         \
                                                                          \
  declare_constant(CodeInstaller::ILLEGAL)                                \
  declare_constant(CodeInstaller::REGISTER_PRIMITIVE)                     \
  declare_constant(CodeInstaller::REGISTER_OOP)                           \
  declare_constant(CodeInstaller::REGISTER_NARROW_OOP)                    \
  declare_constant(CodeInstaller::REGISTER_VECTOR)                        \
  declare_constant(CodeInstaller::STACK_SLOT_PRIMITIVE)                   \
  declare_constant(CodeInstaller::STACK_SLOT_OOP)                         \
  declare_constant(CodeInstaller::STACK_SLOT_NARROW_OOP)                  \
  declare_constant(CodeInstaller::STACK_SLOT_VECTOR)                      \
  declare_constant(CodeInstaller::STACK_SLOT4_PRIMITIVE)                  \
  declare_constant(CodeInstaller::STACK_SLOT4_OOP)                        \
  declare_constant(CodeInstaller::STACK_SLOT4_NARROW_OOP)                 \
  declare_constant(CodeInstaller::STACK_SLOT4_VECTOR)                     \
  declare_constant(CodeInstaller::VIRTUAL_OBJECT_ID)                      \
  declare_constant(CodeInstaller::VIRTUAL_OBJECT_ID2)                     \
  declare_constant(CodeInstaller::NULL_CONSTANT)                          \
  declare_constant(CodeInstaller::RAW_CONSTANT)                           \
  declare_constant(CodeInstaller::PRIMITIVE_0)                            \
  declare_constant(CodeInstaller::PRIMITIVE4)                             \
  declare_constant(CodeInstaller::PRIMITIVE8)                             \
  declare_constant(CodeInstaller::JOBJECT)                                \
  declare_constant(CodeInstaller::OBJECT_ID)                              \
  declare_constant(CodeInstaller::OBJECT_ID2)                             \
                                                                          \
  declare_constant(CodeInstaller::NO_FINALIZABLE_SUBCLASS)                \
  declare_constant(CodeInstaller::CONCRETE_SUBTYPE)                       \
  declare_constant(CodeInstaller::LEAF_TYPE)                              \
  declare_constant(CodeInstaller::CONCRETE_METHOD)                        \
  declare_constant(CodeInstaller::CALLSITE_TARGET_VALUE)                  \
                                                                          \
  declare_constant(CodeInstaller::PATCH_OBJECT_ID)                        \
  declare_constant(CodeInstaller::PATCH_OBJECT_ID2)                       \
  declare_constant(CodeInstaller::PATCH_NARROW_OBJECT_ID)                 \
  declare_constant(CodeInstaller::PATCH_NARROW_OBJECT_ID2)                \
  declare_constant(CodeInstaller::PATCH_JOBJECT)                          \
  declare_constant(CodeInstaller::PATCH_NARROW_JOBJECT)                   \
  declare_constant(CodeInstaller::PATCH_KLASS)                            \
  declare_constant(CodeInstaller::PATCH_NARROW_KLASS)                     \
  declare_constant(CodeInstaller::PATCH_METHOD)                           \
  declare_constant(CodeInstaller::PATCH_DATA_SECTION_REFERENCE)           \
                                                                          \
  declare_constant(CodeInstaller::SITE_CALL)                              \
  declare_constant(CodeInstaller::SITE_FOREIGN_CALL)                      \
  declare_constant(CodeInstaller::SITE_FOREIGN_CALL_NO_DEBUG_INFO)        \
  declare_constant(CodeInstaller::SITE_SAFEPOINT)                         \
  declare_constant(CodeInstaller::SITE_INFOPOINT)                         \
  declare_constant(CodeInstaller::SITE_IMPLICIT_EXCEPTION)                \
  declare_constant(CodeInstaller::SITE_IMPLICIT_EXCEPTION_DISPATCH)       \
  declare_constant(CodeInstaller::SITE_MARK)                              \
  declare_constant(CodeInstaller::SITE_DATA_PATCH)                        \
  declare_constant(CodeInstaller::SITE_EXCEPTION_HANDLER)                 \
                                                                          \
  declare_constant(CodeInstaller::DI_HAS_REFERENCE_MAP)                   \
  declare_constant(CodeInstaller::DI_HAS_CALLEE_SAVE_INFO)                \
  declare_constant(CodeInstaller::DI_HAS_FRAMES)                          \
                                                                          \
  declare_constant(CodeInstaller::DIF_HAS_LOCALS)                         \
  declare_constant(CodeInstaller::DIF_HAS_STACK)                          \
  declare_constant(CodeInstaller::DIF_HAS_LOCKS)                          \
  declare_constant(CodeInstaller::DIF_DURING_CALL)                        \
  declare_constant(CodeInstaller::DIF_RETHROW_EXCEPTION)                  \
                                                                          \
  declare_constant(CodeInstaller::HCC_IS_NMETHOD)                         \
  declare_constant(CodeInstaller::HCC_HAS_ASSUMPTIONS)                    \
  declare_constant(CodeInstaller::HCC_HAS_METHODS)                        \
  declare_constant(CodeInstaller::HCC_HAS_DEOPT_RESCUE_SLOT)              \
  declare_constant(CodeInstaller::HCC_HAS_COMMENTS)                       \
                                                                          \
  declare_constant(CodeInstaller::NO_REGISTER)                            \
                                                                          \
  declare_constant(CollectedHeap::None)                                   \
  declare_constant(CollectedHeap::Serial)                                 \
  declare_constant(CollectedHeap::Parallel)                               \
  declare_constant(CollectedHeap::G1)                                     \
  declare_constant(CollectedHeap::Epsilon)                                \
  declare_constant(CollectedHeap::Z)                                      \
  declare_constant(CollectedHeap::Shenandoah)                             \
                                                                          \
  declare_constant(vmIntrinsics::FIRST_MH_SIG_POLY)                       \
  declare_constant(vmIntrinsics::LAST_MH_SIG_POLY)                        \
  declare_constant(vmIntrinsics::_invokeGeneric)                          \
  declare_constant(vmIntrinsics::_compiledLambdaForm)                     \
                                                                          \
  declare_constant(ConstantPool::_has_dynamic_constant)                   \
                                                                          \
  declare_constant(ConstMethodFlags::_misc_has_linenumber_table)          \
  declare_constant(ConstMethodFlags::_misc_has_localvariable_table)       \
  declare_constant(ConstMethodFlags::_misc_has_exception_table)           \
  declare_constant(ConstMethodFlags::_misc_has_method_annotations)        \
  declare_constant(ConstMethodFlags::_misc_has_parameter_annotations)     \
  declare_constant(ConstMethodFlags::_misc_caller_sensitive)              \
  declare_constant(ConstMethodFlags::_misc_is_hidden)                     \
  declare_constant(ConstMethodFlags::_misc_intrinsic_candidate)           \
  declare_constant(ConstMethodFlags::_misc_reserved_stack_access)         \
  declare_constant(ConstMethodFlags::_misc_changes_current_thread)        \
  declare_constant(ConstMethodFlags::_misc_is_scoped)                     \
  declare_constant(ConstMethodFlags::_misc_is_overpass)                   \
                                                                          \
  declare_constant(CounterData::count_off)                                \
                                                                          \
  declare_constant(DataLayout::cell_size)                                 \
  declare_constant(DataLayout::no_tag)                                    \
  declare_constant(DataLayout::bit_data_tag)                              \
  declare_constant(DataLayout::counter_data_tag)                          \
  declare_constant(DataLayout::jump_data_tag)                             \
  declare_constant(DataLayout::receiver_type_data_tag)                    \
  declare_constant(DataLayout::virtual_call_data_tag)                     \
  declare_constant(DataLayout::ret_data_tag)                              \
  declare_constant(DataLayout::branch_data_tag)                           \
  declare_constant(DataLayout::multi_branch_data_tag)                     \
  declare_constant(DataLayout::arg_info_data_tag)                         \
  declare_constant(DataLayout::call_type_data_tag)                        \
  declare_constant(DataLayout::virtual_call_type_data_tag)                \
  declare_constant(DataLayout::parameters_type_data_tag)                  \
  declare_constant(DataLayout::speculative_trap_data_tag)                 \
                                                                          \
  declare_constant(Deoptimization::Unpack_deopt)                          \
  declare_constant(Deoptimization::Unpack_exception)                      \
  declare_constant(Deoptimization::Unpack_uncommon_trap)                  \
  declare_constant(Deoptimization::Unpack_reexecute)                      \
                                                                          \
  declare_constant(Deoptimization::_action_bits)                          \
  declare_constant(Deoptimization::_reason_bits)                          \
  declare_constant(Deoptimization::_debug_id_bits)                        \
  declare_constant(Deoptimization::_action_shift)                         \
  declare_constant(Deoptimization::_reason_shift)                         \
  declare_constant(Deoptimization::_debug_id_shift)                       \
                                                                          \
  declare_constant(Deoptimization::Action_none)                           \
  declare_constant(Deoptimization::Action_maybe_recompile)                \
  declare_constant(Deoptimization::Action_reinterpret)                    \
  declare_constant(Deoptimization::Action_make_not_entrant)               \
  declare_constant(Deoptimization::Action_make_not_compilable)            \
                                                                          \
  declare_constant(Deoptimization::Reason_none)                           \
  declare_constant(Deoptimization::Reason_null_check)                     \
  declare_constant(Deoptimization::Reason_range_check)                    \
  declare_constant(Deoptimization::Reason_class_check)                    \
  declare_constant(Deoptimization::Reason_array_check)                    \
  declare_constant(Deoptimization::Reason_unreached0)                     \
  declare_constant(Deoptimization::Reason_constraint)                     \
  declare_constant(Deoptimization::Reason_div0_check)                     \
  declare_constant(Deoptimization::Reason_loop_limit_check)               \
  declare_constant(Deoptimization::Reason_short_running_long_loop)        \
  declare_constant(Deoptimization::Reason_auto_vectorization_check)       \
  declare_constant(Deoptimization::Reason_type_checked_inlining)          \
  declare_constant(Deoptimization::Reason_optimized_type_check)           \
  declare_constant(Deoptimization::Reason_aliasing)                       \
  declare_constant(Deoptimization::Reason_transfer_to_interpreter)        \
  declare_constant(Deoptimization::Reason_not_compiled_exception_handler) \
  declare_constant(Deoptimization::Reason_unresolved)                     \
  declare_constant(Deoptimization::Reason_jsr_mismatch)                   \
  declare_constant(Deoptimization::Reason_TRAP_HISTORY_LENGTH)            \
  declare_constant(Deoptimization::_support_large_access_byte_array_virtualization) \
                                                                          \
                                                                          \
  declare_constant(InstanceKlass::linked)                                 \
  declare_constant(InstanceKlass::being_initialized)                      \
  declare_constant(InstanceKlass::fully_initialized)                      \
                                                                          \
  declare_constant(LockingMode::LM_MONITOR)                               \
  declare_constant(LockingMode::LM_LEGACY)                                \
  declare_constant(LockingMode::LM_LIGHTWEIGHT)                           \
                                                                          \
  /*********************************/                                     \
  /* InstanceKlass _misc_flags */                                         \
  /*********************************/                                     \
                                                                          \
  declare_constant(InstanceKlassFlags::_misc_has_nonstatic_concrete_methods)   \
  declare_constant(InstanceKlassFlags::_misc_declares_nonstatic_concrete_methods) \
  declare_constant(KlassFlags::_misc_is_hidden_class)                     \
  declare_constant(KlassFlags::_misc_is_value_based_class)                \
  declare_constant(KlassFlags::_misc_has_finalizer)                       \
  declare_constant(KlassFlags::_misc_is_cloneable_fast)                   \
                                                                          \
  declare_constant(JumpData::taken_off_set)                               \
  declare_constant(JumpData::displacement_off_set)                        \
                                                                          \
  declare_preprocessor_constant("JVMCI::ok",                      JVMCI::ok)                      \
  declare_preprocessor_constant("JVMCI::dependencies_failed",     JVMCI::dependencies_failed)     \
  declare_preprocessor_constant("JVMCI::cache_full",              JVMCI::cache_full)              \
  declare_preprocessor_constant("JVMCI::code_too_large",          JVMCI::code_too_large)          \
  declare_preprocessor_constant("JVMCI::nmethod_reclaimed",       JVMCI::nmethod_reclaimed)       \
  declare_preprocessor_constant("JVMCI::first_permanent_bailout", JVMCI::first_permanent_bailout) \
                                                                          \
  declare_constant(JVMCIRuntime::none)                                    \
  declare_constant(JVMCIRuntime::by_holder)                               \
  declare_constant(JVMCIRuntime::by_full_signature)                       \
                                                                          \
  declare_constant(Klass::_lh_neutral_value)                              \
  declare_constant(Klass::_lh_instance_slow_path_bit)                     \
  declare_constant(Klass::_lh_log2_element_size_shift)                    \
  declare_constant(Klass::_lh_log2_element_size_mask)                     \
  declare_constant(Klass::_lh_element_type_shift)                         \
  declare_constant(Klass::_lh_element_type_mask)                          \
  declare_constant(Klass::_lh_header_size_shift)                          \
  declare_constant(Klass::_lh_header_size_mask)                           \
  declare_constant(Klass::_lh_array_tag_shift)                            \
  declare_constant(Klass::_lh_array_tag_type_value)                       \
  declare_constant(Klass::_lh_array_tag_obj_value)                        \
                                                                          \
  declare_constant(markWord::no_hash)                                     \
                                                                          \
  declare_constant(MethodFlags::_misc_force_inline)                       \
  declare_constant(MethodFlags::_misc_dont_inline)                        \
                                                                          \
  declare_constant(Method::nonvirtual_vtable_index)                       \
  declare_constant(Method::invalid_vtable_index)                          \
                                                                          \
  declare_constant(MultiBranchData::per_case_cell_count)                  \
                                                                          \
  AARCH64_ONLY(declare_constant(NMethodPatchingType::stw_instruction_and_data_patch))  \
  AARCH64_ONLY(declare_constant(NMethodPatchingType::conc_instruction_and_data_patch)) \
  AARCH64_ONLY(declare_constant(NMethodPatchingType::conc_data_patch))                 \
                                                                          \
  declare_constant(ObjectMonitor::NO_OWNER)                               \
  declare_constant(ObjectMonitor::ANONYMOUS_OWNER)                        \
  declare_constant(ObjectMonitor::DEFLATER_MARKER)                        \
                                                                          \
  declare_constant(ReceiverTypeData::receiver_type_row_cell_count)        \
  declare_constant(ReceiverTypeData::receiver0_offset)                    \
  declare_constant(ReceiverTypeData::count0_offset)                       \
                                                                          \
  declare_constant(vmIntrinsics::_invokeBasic)                            \
  declare_constant(vmIntrinsics::_linkToVirtual)                          \
  declare_constant(vmIntrinsics::_linkToStatic)                           \
  declare_constant(vmIntrinsics::_linkToSpecial)                          \
  declare_constant(vmIntrinsics::_linkToInterface)                        \
  declare_constant(vmIntrinsics::_linkToNative)                           \
                                                                          \
  declare_constant(vmSymbols::FIRST_SID)                                  \
  declare_constant(vmSymbols::SID_LIMIT)                                  \

#define VM_LONG_CONSTANTS(declare_constant, declare_preprocessor_constant) \
  declare_constant(InvocationCounter::count_increment)                    \
  declare_constant(InvocationCounter::count_shift)                        \
                                                                          \
  declare_constant(markWord::klass_shift)                                 \
  declare_constant(markWord::hash_shift)                                  \
  declare_constant(markWord::monitor_value)                               \
                                                                          \
  declare_constant(markWord::lock_mask_in_place)                          \
  declare_constant(markWord::age_mask_in_place)                           \
  declare_constant(markWord::hash_mask)                                   \
  declare_constant(markWord::hash_mask_in_place)                          \
                                                                          \
  declare_constant(markWord::unlocked_value)                              \
  declare_constant(markWord::marked_value)                                \
                                                                          \
  declare_constant(markWord::no_hash_in_place)                            \
  declare_constant(markWord::no_lock_in_place)                            \

// Helper macro to support ZGC pattern where the function itself isn't exported
#define DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, name) \
  declare_function_with_value(name, name##_addr())


#define VM_ADDRESSES(declare_address, declare_preprocessor_address, declare_function, declare_function_with_value) \
  declare_function(SharedRuntime::register_finalizer)                     \
  declare_function(SharedRuntime::exception_handler_for_return_address)   \
  declare_function(SharedRuntime::OSR_migration_end)                      \
  declare_function(SharedRuntime::enable_stack_reserved_zone)             \
  declare_function(SharedRuntime::frem)                                   \
  declare_function(SharedRuntime::drem)                                   \
  JVMTI_ONLY(declare_function(SharedRuntime::notify_jvmti_vthread_start)) \
  JVMTI_ONLY(declare_function(SharedRuntime::notify_jvmti_vthread_end))   \
  JVMTI_ONLY(declare_function(SharedRuntime::notify_jvmti_vthread_mount)) \
  JVMTI_ONLY(declare_function(SharedRuntime::notify_jvmti_vthread_unmount)) \
                                                                          \
  declare_function(os::dll_load)                                          \
  declare_function(os::dll_lookup)                                        \
  declare_function(os::javaTimeMillis)                                    \
  declare_function(os::javaTimeNanos)                                     \
                                                                          \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::load_barrier_on_oop_field_preloaded))                      \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::load_barrier_on_weak_oop_field_preloaded))                 \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::load_barrier_on_phantom_oop_field_preloaded))              \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::load_barrier_on_oop_field_preloaded_store_good))           \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::no_keepalive_load_barrier_on_weak_oop_field_preloaded))    \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::no_keepalive_load_barrier_on_phantom_oop_field_preloaded)) \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::no_keepalive_store_barrier_on_oop_field_without_healing))  \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::store_barrier_on_native_oop_field_without_healing))        \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::store_barrier_on_oop_field_with_healing))                  \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::store_barrier_on_oop_field_without_healing))               \
  ZGC_ONLY(DECLARE_FUNCTION_FROM_ADDR(declare_function_with_value, ZBarrierSetRuntime::load_barrier_on_oop_array))                                \
                                                                          \
  declare_function(Deoptimization::fetch_unroll_info)                     \
  declare_function(Deoptimization::uncommon_trap)                         \
  declare_function(Deoptimization::unpack_frames)                         \
                                                                          \
  declare_function(JVMCIRuntime::new_instance_or_null)                    \
  declare_function(JVMCIRuntime::new_array_or_null)                       \
  declare_function(JVMCIRuntime::new_multi_array_or_null)                 \
  declare_function(JVMCIRuntime::dynamic_new_array_or_null)               \
  declare_function(JVMCIRuntime::dynamic_new_instance_or_null)            \
                                                                          \
  declare_function(JVMCIRuntime::invoke_static_method_one_arg)            \
                                                                          \
  declare_function(JVMCIRuntime::vm_message)                              \
  declare_function(JVMCIRuntime::identity_hash_code)                      \
  declare_function(JVMCIRuntime::exception_handler_for_pc)                \
  declare_function(JVMCIRuntime::monitorenter)                            \
  declare_function(JVMCIRuntime::monitorexit)                             \
  declare_function(JVMCIRuntime::object_notify)                           \
  declare_function(JVMCIRuntime::object_notifyAll)                        \
  declare_function(JVMCIRuntime::throw_and_post_jvmti_exception)          \
  declare_function(JVMCIRuntime::throw_klass_external_name_exception)     \
  declare_function(JVMCIRuntime::throw_class_cast_exception)              \
  declare_function(JVMCIRuntime::log_primitive)                           \
  declare_function(JVMCIRuntime::log_object)                              \
  declare_function(JVMCIRuntime::log_printf)                              \
  declare_function(JVMCIRuntime::vm_error)                                \
  declare_function(JVMCIRuntime::load_and_clear_exception)                \
  G1GC_ONLY(declare_function(JVMCIRuntime::write_barrier_pre))            \
  G1GC_ONLY(declare_function(JVMCIRuntime::write_barrier_post))           \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::load_reference_barrier_strong))         \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::load_reference_barrier_strong_narrow))  \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::load_reference_barrier_weak))           \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::load_reference_barrier_weak_narrow))    \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::load_reference_barrier_phantom))        \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::load_reference_barrier_phantom_narrow)) \
  SHENANDOAHGC_ONLY(declare_function(ShenandoahRuntime::write_barrier_pre))                     \
  declare_function(JVMCIRuntime::validate_object)                         \
                                                                          \
  declare_function(JVMCIRuntime::test_deoptimize_call_int)


#if INCLUDE_G1GC

#define VM_STRUCTS_JVMCI_G1GC(nonstatic_field, static_field) \
  static_field(G1HeapRegion, LogOfHRGrainBytes, uint)

#define VM_INT_CONSTANTS_JVMCI_G1GC(declare_constant, declare_constant_with_value, declare_preprocessor_constant) \
  declare_constant_with_value("G1CardTable::g1_young_gen", G1CardTable::g1_young_card_val()) \
  declare_constant_with_value("G1ThreadLocalData::satb_mark_queue_active_offset", in_bytes(G1ThreadLocalData::satb_mark_queue_active_offset())) \
  declare_constant_with_value("G1ThreadLocalData::satb_mark_queue_index_offset", in_bytes(G1ThreadLocalData::satb_mark_queue_index_offset())) \
  declare_constant_with_value("G1ThreadLocalData::satb_mark_queue_buffer_offset", in_bytes(G1ThreadLocalData::satb_mark_queue_buffer_offset())) \
  declare_constant_with_value("G1ThreadLocalData::dirty_card_queue_index_offset", in_bytes(G1ThreadLocalData::dirty_card_queue_index_offset())) \
  declare_constant_with_value("G1ThreadLocalData::dirty_card_queue_buffer_offset", in_bytes(G1ThreadLocalData::dirty_card_queue_buffer_offset()))

#endif // INCLUDE_G1GC


#if INCLUDE_ZGC

#define VM_INT_CONSTANTS_JVMCI_ZGC(declare_constant, declare_constant_with_value, declare_preprocessor_constant)                           \
  declare_constant_with_value("ZThreadLocalData::store_good_mask_offset" , in_bytes(ZThreadLocalData::store_good_mask_offset()))           \
  declare_constant_with_value("ZThreadLocalData::store_bad_mask_offset" , in_bytes(ZThreadLocalData::store_bad_mask_offset()))             \
  declare_constant_with_value("ZThreadLocalData::store_barrier_buffer_offset" , in_bytes(ZThreadLocalData::store_barrier_buffer_offset())) \
  declare_constant_with_value("ZStoreBarrierBuffer::current_offset" , in_bytes(ZStoreBarrierBuffer::current_offset()))                     \
  declare_constant_with_value("ZStoreBarrierBuffer::buffer_offset" , in_bytes(ZStoreBarrierBuffer::buffer_offset()))                       \
  declare_constant_with_value("ZStoreBarrierEntry::p_offset" , in_bytes(ZStoreBarrierEntry::p_offset()))                                   \
  declare_constant_with_value("ZStoreBarrierEntry::prev_offset" , in_bytes(ZStoreBarrierEntry::prev_offset()))                             \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_LOAD_GOOD_BEFORE_SHL))                                            \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_LOAD_BAD_AFTER_TEST))                                             \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_MARK_BAD_AFTER_TEST))                                             \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_STORE_GOOD_AFTER_CMP))                                            \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_STORE_BAD_AFTER_TEST))                                            \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_STORE_GOOD_AFTER_OR))                                             \
  AMD64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_STORE_GOOD_AFTER_MOV))                                            \
  AARCH64_ONLY(declare_constant(ZPointerLoadShift))                                                                                        \
  AARCH64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_LOAD_GOOD_BEFORE_TB_X))                                         \
  AARCH64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_MARK_BAD_BEFORE_MOV))                                           \
  AARCH64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_STORE_GOOD_BEFORE_MOV))                                         \
  AARCH64_ONLY(declare_constant(CodeInstaller::Z_BARRIER_RELOCATION_FORMAT_STORE_BAD_BEFORE_MOV))

#endif // INCLUDE_ZGC

#if INCLUDE_SHENANDOAHGC

#define VM_INT_CONSTANTS_JVMCI_SHENANDOAH(declare_constant, declare_constant_with_value, declare_preprocessor_constant) \
   declare_constant_with_value("ShenandoahThreadLocalData::gc_state_offset", in_bytes(ShenandoahThreadLocalData::gc_state_offset())) \
   declare_constant_with_value("ShenandoahThreadLocalData::satb_mark_queue_index_offset", in_bytes(ShenandoahThreadLocalData::satb_mark_queue_index_offset())) \
   declare_constant_with_value("ShenandoahThreadLocalData::satb_mark_queue_buffer_offset", in_bytes(ShenandoahThreadLocalData::satb_mark_queue_buffer_offset())) \
   declare_constant_with_value("ShenandoahThreadLocalData::card_table_offset", in_bytes(ShenandoahThreadLocalData::card_table_offset())) \
   declare_constant_with_value("ShenandoahHeap::HAS_FORWARDED", ShenandoahHeap::HAS_FORWARDED)                                           \
   declare_constant_with_value("ShenandoahHeap::MARKING", ShenandoahHeap::MARKING)                                                       \
   declare_constant_with_value("ShenandoahHeap::EVACUATION", ShenandoahHeap::EVACUATION)                                                 \
   declare_constant_with_value("ShenandoahHeap::UPDATE_REFS", ShenandoahHeap::UPDATE_REFS)                                               \
   declare_constant_with_value("ShenandoahHeap::WEAK_ROOTS", ShenandoahHeap::WEAK_ROOTS)                                                 \
   declare_constant_with_value("ShenandoahHeap::YOUNG_MARKING", ShenandoahHeap::YOUNG_MARKING)                                           \
   declare_constant_with_value("ShenandoahHeap::OLD_MARKING", ShenandoahHeap::OLD_MARKING)                                               \

#endif

#ifdef LINUX

#define VM_ADDRESSES_OS(declare_address, declare_preprocessor_address, declare_function, declare_function_with_value) \
  declare_preprocessor_address("RTLD_DEFAULT", RTLD_DEFAULT)

#endif


#ifdef BSD

#define VM_ADDRESSES_OS(declare_address, declare_preprocessor_address, declare_function, declare_function_with_value) \
  declare_preprocessor_address("RTLD_DEFAULT", RTLD_DEFAULT)

#endif

#ifdef AARCH64

#define VM_STRUCTS_CPU(nonstatic_field, static_field, unchecked_nonstatic_field, volatile_nonstatic_field, nonproduct_nonstatic_field) \
  static_field(VM_Version, _zva_length, int)                            \
  static_field(StubRoutines::aarch64, _count_positives, address)        \
  static_field(StubRoutines::aarch64, _count_positives_long, address)   \
  static_field(VM_Version, _rop_protection, bool)                       \
  volatile_nonstatic_field(JavaFrameAnchor, _last_Java_fp, intptr_t*)

#define DECLARE_INT_CPU_FEATURE_CONSTANT(id, name, bit) GENERATE_VM_INT_CONSTANT_ENTRY(VM_Version::CPU_##id)
#define VM_INT_CPU_FEATURE_CONSTANTS CPU_FEATURE_FLAGS(DECLARE_INT_CPU_FEATURE_CONSTANT)

#endif

#ifdef X86

#define VM_STRUCTS_CPU(nonstatic_field, static_field, unchecked_nonstatic_field, volatile_nonstatic_field, nonproduct_nonstatic_field) \
  volatile_nonstatic_field(JavaFrameAnchor, _last_Java_fp, intptr_t*) \
  static_field(VM_Version,                     _features,                      VM_Version::VM_Features) \
                                                                                                        \
  nonstatic_field(VM_Version::VM_Features,     _features_bitmap[0],            uint64_t)                \
  static_field(VM_Version::VM_Features,        _features_bitmap_size,          int)                     \
  static_field(VM_Version,                     _has_intel_jcc_erratum,         bool)

#define VM_INT_CONSTANTS_CPU(declare_constant, declare_preprocessor_constant) \
  LP64_ONLY(declare_constant(frame::arg_reg_save_area_bytes))       \
  declare_constant(frame::interpreter_frame_sender_sp_offset)       \
  declare_constant(frame::interpreter_frame_last_sp_offset)

#define DECLARE_LONG_CPU_FEATURE_CONSTANT(id, name, bit) GENERATE_VM_LONG_CONSTANT_ENTRY(VM_Version::CPU_##id)
#define VM_LONG_CPU_FEATURE_CONSTANTS \
   CPU_FEATURE_FLAGS(DECLARE_LONG_CPU_FEATURE_CONSTANT)

#endif

/*
 * Dummy defines for architectures that don't use these.
 */
#ifndef VM_STRUCTS_CPU
#define VM_STRUCTS_CPU(nonstatic_field, static_field, unchecked_nonstatic_field, volatile_nonstatic_field, nonproduct_nonstatic_field)
#endif

#ifndef VM_INT_CONSTANTS_CPU
#define VM_INT_CONSTANTS_CPU(declare_constant, declare_preprocessor_constant)
#endif

#ifndef VM_LONG_CONSTANTS_CPU
#define VM_LONG_CONSTANTS_CPU(declare_constant, declare_preprocessor_constant)
#endif

#ifndef VM_ADDRESSES_OS
#define VM_ADDRESSES_OS(declare_address, declare_preprocessor_address, declare_function, declare_function_with_value)
#endif

//
// Instantiation of VMStructEntries, VMTypeEntries and VMIntConstantEntries
//

#define GENERATE_VM_FUNCTION_WITH_VALUE_ENTRY(name, value) \
  { QUOTE(name), CAST_FROM_FN_PTR(void*, value) },


// These initializers are allowed to access private fields in classes
// as long as class VMStructs is a friend
VMStructEntry JVMCIVMStructs::localHotSpotVMStructs[] = {
  VM_STRUCTS(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_STATIC_VM_STRUCT_ENTRY,
             GENERATE_UNCHECKED_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_NONSTATIC_VM_STRUCT_ENTRY)

  VM_STRUCTS_CPU(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_STATIC_VM_STRUCT_ENTRY,
                 GENERATE_UNCHECKED_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY)

#if INCLUDE_G1GC
  VM_STRUCTS_JVMCI_G1GC(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                        GENERATE_STATIC_VM_STRUCT_ENTRY)
#endif

  GENERATE_VM_STRUCT_LAST_ENTRY()
};

VMTypeEntry JVMCIVMStructs::localHotSpotVMTypes[] = {
  VM_TYPES(GENERATE_VM_TYPE_ENTRY,
           GENERATE_TOPLEVEL_VM_TYPE_ENTRY,
           GENERATE_INTEGER_VM_TYPE_ENTRY,
           GENERATE_UNSIGNED_INTEGER_VM_TYPE_ENTRY)

  GENERATE_VM_TYPE_LAST_ENTRY()
};

VMIntConstantEntry JVMCIVMStructs::localHotSpotVMIntConstants[] = {
  VM_INT_CONSTANTS(GENERATE_VM_INT_CONSTANT_ENTRY,
                   GENERATE_VM_INT_CONSTANT_WITH_VALUE_ENTRY,
                   GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)

  VM_INT_CONSTANTS_CPU(GENERATE_VM_INT_CONSTANT_ENTRY,
                       GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)

#if INCLUDE_G1GC
  VM_INT_CONSTANTS_JVMCI_G1GC(GENERATE_VM_INT_CONSTANT_ENTRY,
                              GENERATE_VM_INT_CONSTANT_WITH_VALUE_ENTRY,
                              GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)
#endif
#if INCLUDE_ZGC
  VM_INT_CONSTANTS_JVMCI_ZGC(GENERATE_VM_INT_CONSTANT_ENTRY,
                              GENERATE_VM_INT_CONSTANT_WITH_VALUE_ENTRY,
                              GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)
#endif
#if INCLUDE_SHENANDOAHGC
  VM_INT_CONSTANTS_JVMCI_SHENANDOAH(GENERATE_VM_INT_CONSTANT_ENTRY,
                                    GENERATE_VM_INT_CONSTANT_WITH_VALUE_ENTRY,
                                    GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)
#endif
#ifdef VM_INT_CPU_FEATURE_CONSTANTS
  VM_INT_CPU_FEATURE_CONSTANTS
#endif
  GENERATE_VM_INT_CONSTANT_LAST_ENTRY()
};

VMLongConstantEntry JVMCIVMStructs::localHotSpotVMLongConstants[] = {
  VM_LONG_CONSTANTS(GENERATE_VM_LONG_CONSTANT_ENTRY,
                    GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY)

  VM_LONG_CONSTANTS_CPU(GENERATE_VM_LONG_CONSTANT_ENTRY,
                        GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY)
#ifdef VM_LONG_CPU_FEATURE_CONSTANTS
  VM_LONG_CPU_FEATURE_CONSTANTS
#endif
  GENERATE_VM_LONG_CONSTANT_LAST_ENTRY()
};

VMAddressEntry JVMCIVMStructs::localHotSpotVMAddresses[] = {
  VM_ADDRESSES(GENERATE_VM_ADDRESS_ENTRY,
               GENERATE_PREPROCESSOR_VM_ADDRESS_ENTRY,
               GENERATE_VM_FUNCTION_ENTRY,
               GENERATE_VM_FUNCTION_WITH_VALUE_ENTRY)
  VM_ADDRESSES_OS(GENERATE_VM_ADDRESS_ENTRY,
                  GENERATE_PREPROCESSOR_VM_ADDRESS_ENTRY,
                  GENERATE_VM_FUNCTION_ENTRY,
                  GENERATE_VM_FUNCTION_WITH_VALUE_ENTRY)

  GENERATE_VM_ADDRESS_LAST_ENTRY()
};

int JVMCIVMStructs::localHotSpotVMStructs_count() {
  // Ignore sentinel entry at the end
  return (sizeof(localHotSpotVMStructs) / sizeof(VMStructEntry)) - 1;
}
int JVMCIVMStructs::localHotSpotVMTypes_count() {
  // Ignore sentinel entry at the end
  return (sizeof(localHotSpotVMTypes) / sizeof(VMTypeEntry)) - 1;
}
int JVMCIVMStructs::localHotSpotVMIntConstants_count() {
  // Ignore sentinel entry at the end
  return (sizeof(localHotSpotVMIntConstants) / sizeof(VMIntConstantEntry)) - 1;
}
int JVMCIVMStructs::localHotSpotVMLongConstants_count() {
  // Ignore sentinel entry at the end
  return (sizeof(localHotSpotVMLongConstants) / sizeof(VMLongConstantEntry)) - 1;
}
int JVMCIVMStructs::localHotSpotVMAddresses_count() {
  // Ignore sentinel entry at the end
  return (sizeof(localHotSpotVMAddresses) / sizeof(VMAddressEntry)) - 1;
}

#ifdef ASSERT
// This is used both to check the types of referenced fields and
// to ensure that all of the field types are present.
void JVMCIVMStructs::init() {
  VM_STRUCTS(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_STATIC_VM_STRUCT_ENTRY,
             CHECK_NO_OP,
             CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY);


  VM_STRUCTS_CPU(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
                 CHECK_STATIC_VM_STRUCT_ENTRY,
                 CHECK_NO_OP,
                 CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY,
                 CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY)

#if INCLUDE_G1GC
  VM_STRUCTS_JVMCI_G1GC(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
                        CHECK_STATIC_VM_STRUCT_ENTRY)
#endif

  VM_TYPES(CHECK_VM_TYPE_ENTRY,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP);
}

void jvmci_vmStructs_init() {
  JVMCIVMStructs::init();
}
#endif // ASSERT
