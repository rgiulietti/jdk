/*
 * Copyright (c) 2023, 2025, Oracle and/or its affiliates. All rights reserved.
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
 */

package org.openjdk.bench.java.lang.foreign;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySegment.Scope;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.util.concurrent.TimeUnit;

@BenchmarkMode(Mode.AverageTime)
@Warmup(iterations = 5, time = 500, timeUnit = TimeUnit.MILLISECONDS)
@Measurement(iterations = 10, time = 500, timeUnit = TimeUnit.MILLISECONDS)
@State(org.openjdk.jmh.annotations.Scope.Thread)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@Fork(value = 3, jvmArgs = { "--enable-native-access=ALL-UNNAMED", "--add-opens=java.base/jdk.internal.misc=ALL-UNNAMED" })
public class AllocTest extends CLayouts {

    @Param({"5", "20", "100", "500", "2000", "8000"})
    public int size;

    @Benchmark
    public long alloc_confined() {
        try (Arena arena = Arena.ofConfined()) {
            return arena.allocate(size).address();
        }
    }

    @Benchmark
    public long alloc_calloc_arena() {
        try (CallocArena arena = new CallocArena()) {
            return arena.allocate(size).address();
        }
    }

    @Benchmark
    public long alloc_unsafe_arena() {
        try (UnsafeArena arena = new UnsafeArena()) {
            return arena.allocate(size).address();
        }
    }

    private static class CallocArena implements Arena {

        static final MethodHandle CALLOC = Linker.nativeLinker()
                .downcallHandle(
                        Linker.nativeLinker().defaultLookup().findOrThrow("calloc"),
                        FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.JAVA_LONG, ValueLayout.JAVA_LONG));

        static MemorySegment calloc(long size) {
            try {
                return (MemorySegment)CALLOC.invokeExact(size, 1L);
            } catch (Throwable ex) {
                throw new IllegalStateException(ex);
            }
        }

        final Arena arena = Arena.ofConfined();

        @Override
        public Scope scope() {
            return arena.scope();
        }

        @Override
        public void close() {
            arena.close();
        }

        @Override
        public MemorySegment allocate(long byteSize, long byteAlignment) {
            return calloc(byteSize)
                    .reinterpret(byteSize, arena, CLayouts::freeMemory);
        }
    }

    private static class UnsafeArena implements Arena {

        final Arena arena = Arena.ofConfined();

        @Override
        public Scope scope() {
            return arena.scope();
        }

        @Override
        public void close() {
            arena.close();
        }

        @Override
        public MemorySegment allocate(long byteSize, long byteAlignment) {
            long address = Utils.unsafe.allocateMemory(byteSize);
            Utils.unsafe.setMemory(address, byteSize, (byte)0);
            return MemorySegment.ofAddress(address).reinterpret(byteSize, arena, ms -> Utils.unsafe.freeMemory(ms.address()));
        }
    }
}
