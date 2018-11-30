/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.filament.filamat;

import android.support.annotation.NonNull;
import java.nio.ByteBuffer;

public class MaterialBuilder {

    private final BuilderFinalizer mFinalizer;
    private final long mNativeObject;

    public MaterialBuilder() {
        mNativeObject = nCreateMaterialBuilder();
        mFinalizer = new BuilderFinalizer(mNativeObject);
    }

    @NonNull
    public MaterialBuilder name(@NonNull String name) {
        nMaterialBuilderName(mNativeObject, name);
        return this;
    }

    @NonNull
    public MaterialPackage build() {
        long nativePackage = nBuilderBuild(mNativeObject);
        // todo: is it even possible to return 0?
        if (nativePackage == 0) throw new IllegalStateException("Couldn't build Material");
        byte[] data = nGetPackageBytes(nativePackage);
        MaterialPackage result =
                new MaterialPackage(ByteBuffer.wrap(data), nGetPackageIsValid(nativePackage));
        nDestroyPackage(nativePackage);
        return result;
    }

    private static class BuilderFinalizer {
        private final long mNativeObject;

        BuilderFinalizer(long nativeObject) {
            mNativeObject = nativeObject;
        }

        @Override
        public void finalize() {
            try {
                super.finalize();
            } catch (Throwable t) { // Ignore
            } finally {
                nDestroyMaterialBuilder(mNativeObject);
            }
        }
    }

    private static native long nCreateMaterialBuilder();
    private static native void nDestroyMaterialBuilder(long nativeBuilder);

    private static native long nBuilderBuild(long nativeMaterialBuilder);
    private static native byte[] nGetPackageBytes(long nativePackage);
    private static native boolean nGetPackageIsValid(long nativePackage);
    private static native void nDestroyPackage(long nativePackage);

    private static native void nMaterialBuilderName(long nativeMaterialBuilder, String name);
}

