/*
 * Copyright (c) 2019, 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package javax.lang.model.util;

import java.util.List;
import java.util.ArrayList;
import javax.lang.model.element.*;
import javax.annotation.processing.SupportedSourceVersion;
import javax.lang.model.SourceVersion;
import static javax.lang.model.SourceVersion.*;

/**
 * A scanning visitor of program elements with default behavior
 * appropriate for the {@link SourceVersion#RELEASE_14 RELEASE_14}
 * source version.
 *
 * The <code>visit<i>Xyz</i></code> methods in this class scan their
 * component elements by calling {@link ElementScanner6#scan(Element,
 * Object) scan} on their {@linkplain Element#getEnclosedElements
 * enclosed elements}, {@linkplain ExecutableElement#getParameters
 * parameters}, etc., as indicated in the individual method
 * specifications.  A subclass can control the order elements are
 * visited by overriding the <code>visit<i>Xyz</i></code> methods.
 * Note that clients of a scanner may get the desired behavior by
 * invoking {@code v.scan(e, p)} rather than {@code v.visit(e, p)} on
 * the root objects of interest.
 *
 * <p>When a subclass overrides a <code>visit<i>Xyz</i></code> method, the
 * new method can cause the enclosed elements to be scanned in the
 * default way by calling <code>super.visit<i>Xyz</i></code>.  In this
 * fashion, the concrete visitor can control the ordering of traversal
 * over the component elements with respect to the additional
 * processing; for example, consistently calling
 * <code>super.visit<i>Xyz</i></code> at the start of the overridden
 * methods will yield a preorder traversal, etc.  If the component
 * elements should be traversed in some other order, instead of
 * calling <code>super.visit<i>Xyz</i></code>, an overriding visit method
 * should call {@code scan} with the elements in the desired order.
 *
 * @apiNote
 * Methods in this class may be overridden subject to their general
 * contract.
 *
 * @param <R> the return type of this visitor's methods.  Use {@link
 *            Void} for visitors that do not need to return results.
 * @param <P> the type of the additional parameter to this visitor's
 *            methods.  Use {@code Void} for visitors that do not need an
 *            additional parameter.
 *
 * @see ElementScanner6##note_for_subclasses <strong>Compatibility note for subclasses</strong>
 * @see ElementScanner6
 * @see ElementScanner7
 * @see ElementScanner8
 * @see ElementScanner9
 * @since 16
 */
@SupportedSourceVersion(RELEASE_26)
public class ElementScanner14<R, P> extends ElementScanner9<R, P> {
    /**
     * Constructor for concrete subclasses; uses {@code null} for the
     * default value.
     */
    protected ElementScanner14(){
        super(null);
    }

    /**
     * Constructor for concrete subclasses; uses the argument for the
     * default value.
     *
     * @param defaultValue the default value
     */
    protected ElementScanner14(R defaultValue){
        super(defaultValue);
    }

    /**
     * {@inheritDoc ElementVisitor}
     *
     * @implSpec This implementation scans the type parameters, if
     * any, and then the enclosed elements.
     *
     *
     * @param e  {@inheritDoc ElementVisitor}
     * @param p  {@inheritDoc ElementVisitor}
     * @return   {@inheritDoc ElementScanner6}
     */
    @Override
    public R visitType(TypeElement e, P p) {
        return scan(createScanningList(e, e.getEnclosedElements()), p);
    }

    /**
     * {@inheritDoc ElementVisitor}
     *
     * @implSpec This implementation first scans the type parameters, if any, and then
     * the parameters.
     *
     * @param e  {@inheritDoc ElementVisitor}
     * @param p  {@inheritDoc ElementVisitor}
     * @return   {@inheritDoc ElementScanner6}
     */
    @Override
    public R visitExecutable(ExecutableElement e, P p) {
        return scan(createScanningList(e, e.getParameters()), p);
    }

    private List<? extends Element> createScanningList(Parameterizable element,
                                                       List<? extends Element> toBeScanned) {
        var typeParameters = element.getTypeParameters();
        if (typeParameters.isEmpty()) {
            return toBeScanned;
        } else {
            List<Element> scanningList = new ArrayList<>(typeParameters);
            scanningList.addAll(toBeScanned);
            return scanningList;
        }
    }

    /**
     * {@inheritDoc ElementVisitor}
     *
     * @implSpec This implementation scans the enclosed elements.
     *
     * @param e {@inheritDoc ElementVisitor}
     * @param p {@inheritDoc ElementVisitor}
     * @return  {@inheritDoc ElementScanner6}
     */
    @Override
    public R visitRecordComponent(RecordComponentElement e, P p) {
        return scan(e.getEnclosedElements(), p);
    }
}
