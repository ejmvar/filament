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

#include <jni.h>

#include <filamat/MaterialBuilder.h>

#include <iostream>

using namespace filamat;

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nCreateMaterialBuilder(JNIEnv *env,
        jclass type) {
    std::cout << "Creating a material builder\n";
    return (jlong) new MaterialBuilder();
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nDestroyMaterialBuilder(JNIEnv *env,
        jclass type, jlong nativeBuilder) {
    std::cout << "Destroying the material builder\n";
    auto builder = (MaterialBuilder*) nativeBuilder;
    delete builder;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nBuilderBuild(JNIEnv* env, jclass type,
        jlong nativeBuilder) {
    std::cout << "Build!";
    auto builder = (MaterialBuilder*) nativeBuilder;
    return (jlong) new Package(builder->build());
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nGetPackageBytes(JNIEnv* env, jclass type,
        jlong nativePackage) {
    auto package = (Package*) nativePackage;
    auto size = jsize(package->getSize());
    jbyteArray ret = env->NewByteArray(size);
    auto data = (jbyte*) package->getData();
    env->SetByteArrayRegion(ret, 0, size, data);
    return ret;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nGetPackageIsValid(JNIEnv* env,
        jclass type, jlong nativePackage) {
    auto* package = (Package*) nativePackage;
    return jboolean(package->isValid());
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nDestroyPackage(JNIEnv* env, jclass type,
        jlong nativePackage) {
    Package* package = (Package*) nativePackage;
    delete package;
}

extern "C" JNIEXPORT void JNICALL
Java_com_google_android_filament_filamat_MaterialBuilder_nMaterialBuilderName(JNIEnv* env,
        jclass type, jlong nativeBuilder, jstring name_) {
    auto builder = (MaterialBuilder*) nativeBuilder;
    const char* name = env->GetStringUTFChars(name_, nullptr);
    builder->name(name);
}
