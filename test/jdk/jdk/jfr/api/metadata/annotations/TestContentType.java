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
 */

package jdk.jfr.api.metadata.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

import jdk.jfr.AnnotationElement;
import jdk.jfr.ContentType;
import jdk.jfr.Event;
import jdk.jfr.EventType;
import jdk.jfr.MetadataDefinition;
import jdk.jfr.Timespan;
import jdk.test.lib.Asserts;
import jdk.test.lib.jfr.Events;

/**
 * @test
 * @requires vm.flagless
 * @requires vm.hasJFR
 * @library /test/lib
 * @run main/othervm jdk.jfr.api.metadata.annotations.TestContentType
 */
public class TestContentType {
    @ContentType
    @MetadataDefinition
    @Target({ ElementType.FIELD })
    @Retention(RetentionPolicy.RUNTIME)
    @interface Temperature {
    }

    static class SunnyDay extends Event {
        String day;
        @Temperature
        float max;
        @Timespan
        float hours;
    }

    public static void main(String[] args) throws Exception {

        EventType t = EventType.getEventType(SunnyDay.class);
        AnnotationElement aMax = Events.getAnnotation(t.getField("max"), Temperature.class);
        ContentType cMax = aMax.getAnnotation(ContentType.class);
        if (cMax == null) {
            throw new Exception("Expected Temperature annotation for field 'max' to have meta annotation ContentType");
        }
        AnnotationElement aHours = Events.getAnnotation(t.getField("hours"), Timespan.class);
        ContentType cHours = aHours.getAnnotation(ContentType.class);
        Asserts.assertTrue(cHours != null, "Expected Timespan annotation for field 'hours' to have meta annotation ContentType");
    }
}
