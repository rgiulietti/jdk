/*
 * Copyright (c) 2003, 2025, Oracle and/or its affiliates. All rights reserved.
 */

/* Copyright  (c) 2002 Graz University of Technology. All rights reserved.
 *
 * Redistribution and use in  source and binary forms, with or without
 * modification, are permitted  provided that the following conditions are met:
 *
 * 1. Redistributions of  source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in  binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The end-user documentation included with the redistribution, if any, must
 *    include the following acknowledgment:
 *
 *    "This product includes software developed by IAIK of Graz University of
 *     Technology."
 *
 *    Alternately, this acknowledgment may appear in the software itself, if
 *    and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Graz University of Technology" and "IAIK of Graz University of
 *    Technology" must not be used to endorse or promote products derived from
 *    this software without prior written permission.
 *
 * 5. Products derived from this software may not be called
 *    "IAIK PKCS Wrapper", nor may "IAIK" appear in their name, without prior
 *    written permission of Graz University of Technology.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE LICENSOR BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 *  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 *  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY  OF SUCH DAMAGE.
 */

/*
 * pkcs11wrapper.c
 * 18.05.2001
 *
 * This is the implementation of the native functions of the Java to PKCS#11 interface.
 * All function use some helper functions to convert the JNI types to PKCS#11 types.
 *
 * @author Karl Scheibelhofer <Karl.Scheibelhofer@iaik.at>
 * @author Martin Schlaeffer <schlaeff@sbox.tugraz.at>
 */


#include "pkcs11wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sun_security_pkcs11_wrapper_PKCS11.h"

/* declare file private functions */

CK_VOID_PTR jMechParamToCKMechParamPtrSlow(JNIEnv *env, jobject jParam,
        CK_MECHANISM_TYPE ckMech, CK_ULONG *ckpLength);


/*
 * converts a CK_DATE pointer into a Java CK_DATE Object.
 *
 * @param env - used to call JNI funktions to create the new Java object
 * @param ckpValue - the pointer to the CK_DATE structure
 * @return - the new Java CK_DATE object
 */
jobject ckDatePtrToJDateObject(JNIEnv *env, const CK_DATE *ckpDate)
{
    jclass jDateClass;
    jmethodID jCtrId;
    jobject jDateObject;
    jcharArray jYear;
    jcharArray jMonth;
    jcharArray jDay;

    /* load CK_DATE class */
    jDateClass = (*env)->FindClass(env, CLASS_DATE);
    if (jDateClass == NULL) { return NULL; }

    /* load CK_DATE constructor */
    jCtrId = (*env)->GetMethodID(env, jDateClass, "<init>", "([C[C[C)V");
    if (jCtrId == NULL) { return NULL; }

    /* prep all fields */
    jYear = ckCharArrayToJCharArray(env, (CK_CHAR_PTR)(ckpDate->year), 4);
    if (jYear == NULL) { return NULL; }
    jMonth = ckCharArrayToJCharArray(env, (CK_CHAR_PTR)(ckpDate->month), 2);
    if (jMonth == NULL) { return NULL; }
    jDay = ckCharArrayToJCharArray(env, (CK_CHAR_PTR)(ckpDate->day), 2);
    if (jDay == NULL) { return NULL; }

    /* create new CK_DATE object */
    jDateObject =
      (*env)->NewObject(env, jDateClass, jCtrId, jYear, jMonth, jDay);
    if (jDateObject == NULL) { return NULL; }

    /* free local references */
    (*env)->DeleteLocalRef(env, jDateClass);
    (*env)->DeleteLocalRef(env, jYear);
    (*env)->DeleteLocalRef(env, jMonth);
    (*env)->DeleteLocalRef(env, jDay);

    return jDateObject ;
}

/*
 * converts a CK_VERSION pointer into a Java CK_VERSION Object.
 *
 * @param env - used to call JNI funktions to create the new Java object
 * @param ckpVersion - the pointer to the CK_VERSION structure
 * @return the new Java CK_VERSION object
 */
jobject ckVersionPtrToJVersion(JNIEnv *env, const CK_VERSION_PTR ckpVersion)
{
    jclass jVersionClass;
    jmethodID jCtrId;
    jobject jVersionObject;
    jint jMajor;
    jint jMinor;

    /* load CK_VERSION class */
    jVersionClass = (*env)->FindClass(env, CLASS_VERSION);
    if (jVersionClass == NULL) { return NULL; }

    /* load CK_VERSION constructor */
    jCtrId = (*env)->GetMethodID(env, jVersionClass, "<init>", "(II)V");
    if (jCtrId == NULL) { return NULL; }

    /* prep both fields */
    jMajor = ckpVersion->major;
    jMinor = ckpVersion->minor;

    /* create new CK_VERSION object */
    jVersionObject =
      (*env)->NewObject(env, jVersionClass, jCtrId, jMajor, jMinor);
    if (jVersionObject == NULL) { return NULL; }

    /* free local references */
    (*env)->DeleteLocalRef(env, jVersionClass);

    return jVersionObject ;
}

/*
 * converts a CK_SESSION_INFO pointer into a Java CK_SESSION_INFO Object.
 *
 * @param env - used to call JNI funktions to create the new Java object
 * @param ckpSessionInfo - the pointer to the CK_SESSION_INFO structure
 * @return the new Java CK_SESSION_INFO object
 */
jobject ckSessionInfoPtrToJSessionInfo(JNIEnv *env, const CK_SESSION_INFO_PTR ckpSessionInfo)
{
    jclass jSessionInfoClass;
    jmethodID jCtrId;
    jobject jSessionInfoObject;
    jlong jSlotID;
    jlong jState;
    jlong jFlags;
    jlong jDeviceError;

    /* load CK_SESSION_INFO class */
    jSessionInfoClass = (*env)->FindClass(env, CLASS_SESSION_INFO);
    if (jSessionInfoClass == NULL) { return NULL; }

    /* load CK_SESSION_INFO constructor */
    jCtrId = (*env)->GetMethodID(env, jSessionInfoClass, "<init>", "(JJJJ)V");
    if (jCtrId == NULL) { return NULL; }

    /* prep all fields */
    jSlotID = ckULongToJLong(ckpSessionInfo->slotID);
    jState = ckULongToJLong(ckpSessionInfo->state);
    jFlags = ckULongToJLong(ckpSessionInfo->flags);
    jDeviceError = ckULongToJLong(ckpSessionInfo->ulDeviceError);

    /* create new CK_SESSION_INFO object */
    jSessionInfoObject =
      (*env)->NewObject(env, jSessionInfoClass, jCtrId, jSlotID, jState,
                        jFlags, jDeviceError);
    if (jSessionInfoObject == NULL) { return NULL; }

    /* free local references */
    (*env)->DeleteLocalRef(env, jSessionInfoClass);

    return jSessionInfoObject ;
}

/*
 * converts a CK_ATTRIBUTE pointer into a Java CK_ATTRIBUTE Object.
 *
 * @param env - used to call JNI funktions to create the new Java object
 * @param ckpAttribute - the pointer to the CK_ATTRIBUTE structure
 * @return the new Java CK_ATTRIBUTE object
 */
jobject ckAttributePtrToJAttribute(JNIEnv *env, const CK_ATTRIBUTE_PTR ckpAttribute)
{
    jclass jAttributeClass;
    jmethodID jCtrId;
    jobject jAttributeObject;
    jlong jType;
    jobject jPValue = NULL;

    jAttributeClass = (*env)->FindClass(env, CLASS_ATTRIBUTE);
    if (jAttributeClass == NULL) { return NULL; }

    /* load CK_INFO constructor */
    jCtrId = (*env)->GetMethodID(env, jAttributeClass, "<init>", "(JLjava/lang/Object;)V");
    if (jCtrId == NULL) { return NULL; }

    /* prep both fields */
    jType = ckULongToJLong(ckpAttribute->type);
    jPValue = ckAttributeValueToJObject(env, ckpAttribute);
    if ((*env)->ExceptionCheck(env)) { return NULL; }

    /* create new CK_ATTRIBUTE object */
    jAttributeObject =
      (*env)->NewObject(env, jAttributeClass, jCtrId, jType, jPValue);
    if (jAttributeObject == NULL) { return NULL; }

    /* free local references */
    (*env)->DeleteLocalRef(env, jAttributeClass);
    (*env)->DeleteLocalRef(env, jPValue);

    return jAttributeObject;
}


/*
 * converts a Java CK_VERSION object into a CK_VERSION pointer
 *
 * @param env - used to call JNI funktions to get the values out of the Java object
 * @param jVersion - the Java CK_VERSION object to convert
 * @return pointer to the new CK_VERSION structure
 */
CK_VERSION_PTR
jVersionToCKVersionPtr(JNIEnv *env, jobject jVersion)
{
    CK_VERSION_PTR ckpVersion;
    jclass jVersionClass;
    jfieldID jFieldID;
    jbyte jMajor, jMinor;

    if (jVersion == NULL) {
        return NULL;
    }

    // retrieve java values
    jVersionClass = (*env)->GetObjectClass(env, jVersion);
    if (jVersionClass == NULL) { return NULL; }
    jFieldID = (*env)->GetFieldID(env, jVersionClass, "major", "B");
    if (jFieldID == NULL) { return NULL; }
    jMajor = (*env)->GetByteField(env, jVersion, jFieldID);
    jFieldID = (*env)->GetFieldID(env, jVersionClass, "minor", "B");
    if (jFieldID == NULL) { return NULL; }
    jMinor = (*env)->GetByteField(env, jVersion, jFieldID);

    // allocate memory for CK_VERSION pointer
    ckpVersion = (CK_VERSION_PTR) calloc(1, sizeof(CK_VERSION));
    if (ckpVersion == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckpVersion->major = jByteToCKByte(jMajor);
    ckpVersion->minor = jByteToCKByte(jMinor);

    return ckpVersion;
}


/*
 * converts a Java CK_DATE object into a CK_DATE pointer
 *
 * @param env - used to call JNI functions to get the values out of the Java object
 * @param jDate - the Java CK_DATE object to convert
 * @return pointer to the new CK_DATE structure
 */
CK_DATE * jDateObjectToCKDatePtr(JNIEnv *env, jobject jDate)
{
    CK_DATE * ckpDate = NULL;
    CK_ULONG ckLength;
    jclass jDateClass;
    jfieldID jFieldID;
    jobject jYear, jMonth, jDay;
    jchar *jTempChars = NULL;
    CK_ULONG i;

    if (jDate == NULL) {
        return NULL;
    }

    // retrieve java values
    jDateClass = (*env)->FindClass(env, CLASS_DATE);
    if (jDateClass == NULL) { return NULL; }
    jFieldID = (*env)->GetFieldID(env, jDateClass, "year", "[C");
    if (jFieldID == NULL) { return NULL; }
    jYear = (*env)->GetObjectField(env, jDate, jFieldID);
    jFieldID = (*env)->GetFieldID(env, jDateClass, "month", "[C");
    if (jFieldID == NULL) { return NULL; }
    jMonth = (*env)->GetObjectField(env, jDate, jFieldID);
    jFieldID = (*env)->GetFieldID(env, jDateClass, "day", "[C");
    if (jFieldID == NULL) { return NULL; }
    jDay = (*env)->GetObjectField(env, jDate, jFieldID);

    // allocate memory for CK_DATE pointer
    ckpDate = (CK_DATE *) calloc(1, sizeof(CK_DATE));
    if (ckpDate == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    if (jYear == NULL) {
        ckpDate->year[0] = 0;
        ckpDate->year[1] = 0;
        ckpDate->year[2] = 0;
        ckpDate->year[3] = 0;
    } else {
        ckLength = (*env)->GetArrayLength(env, jYear);
        jTempChars = (jchar*) calloc(ckLength, sizeof(jchar));
        if (jTempChars == NULL) {
            p11ThrowOutOfMemoryError(env, 0);
            goto cleanup;
        }
        (*env)->GetCharArrayRegion(env, jYear, 0, ckLength, jTempChars);
        if ((*env)->ExceptionCheck(env)) {
            goto cleanup;
        }

        for (i = 0; (i < ckLength) && (i < 4) ; i++) {
            ckpDate->year[i] = jCharToCKChar(jTempChars[i]);
        }
        free(jTempChars);
    }

    if (jMonth == NULL) {
        ckpDate->month[0] = 0;
        ckpDate->month[1] = 0;
    } else {
        ckLength = (*env)->GetArrayLength(env, jMonth);
        jTempChars = (jchar*) calloc(ckLength, sizeof(jchar));
        if (jTempChars == NULL) {
            p11ThrowOutOfMemoryError(env, 0);
            goto cleanup;
        }
        (*env)->GetCharArrayRegion(env, jMonth, 0, ckLength, jTempChars);
        if ((*env)->ExceptionCheck(env)) {
            goto cleanup;
        }

        for (i = 0; (i < ckLength) && (i < 2) ; i++) {
            ckpDate->month[i] = jCharToCKChar(jTempChars[i]);
        }
        free(jTempChars);
    }

    if (jDay == NULL) {
        ckpDate->day[0] = 0;
        ckpDate->day[1] = 0;
    } else {
        ckLength = (*env)->GetArrayLength(env, jDay);
        jTempChars = (jchar*) calloc(ckLength, sizeof(jchar));
        if (jTempChars == NULL) {
            p11ThrowOutOfMemoryError(env, 0);
            goto cleanup;
        }
        (*env)->GetCharArrayRegion(env, jDay, 0, ckLength, jTempChars);
        if ((*env)->ExceptionCheck(env)) {
            goto cleanup;
        }

        for (i = 0; (i < ckLength) && (i < 2) ; i++) {
            ckpDate->day[i] = jCharToCKChar(jTempChars[i]);
        }
        free(jTempChars);
    }

    return ckpDate;
cleanup:
    free(jTempChars);
    free(ckpDate);
    return NULL;
}


/*
 * converts a Java CK_ATTRIBUTE object into a CK_ATTRIBUTE structure
 *
 * @param env - used to call JNI funktions to get the values out of the Java object
 * @param jAttribute - the Java CK_ATTRIBUTE object to convert
 * @return the new CK_ATTRIBUTE structure
 */
CK_ATTRIBUTE jAttributeToCKAttribute(JNIEnv *env, jobject jAttribute)
{
    CK_ATTRIBUTE ckAttribute;
    jclass jAttributeClass;
    jfieldID jFieldID;
    jlong jType;
    jobject jPValue;

    memset(&ckAttribute, 0, sizeof(CK_ATTRIBUTE));

    // TBD: what if jAttribute == NULL?!
    TRACE0("\nDEBUG: jAttributeToCKAttribute");

    /* get CK_ATTRIBUTE class */
    TRACE0(", getting attribute object class");
    jAttributeClass = (*env)->GetObjectClass(env, jAttribute);
    if (jAttributeClass == NULL) { return ckAttribute; }

    /* get type */
    TRACE0(", getting type field");
    jFieldID = (*env)->GetFieldID(env, jAttributeClass, "type", "J");
    if (jFieldID == NULL) { return ckAttribute; }
    jType = (*env)->GetLongField(env, jAttribute, jFieldID);
    TRACE1(", type=0x%lX", jType);

    /* get pValue */
    TRACE0(", getting pValue field");
    jFieldID = (*env)->GetFieldID(env, jAttributeClass, "pValue", "Ljava/lang/Object;");
    if (jFieldID == NULL) { return ckAttribute; }
    jPValue = (*env)->GetObjectField(env, jAttribute, jFieldID);
    TRACE1(", pValue=%p", jPValue);

    ckAttribute.type = jLongToCKULong(jType);
    TRACE0(", converting pValue to primitive object");

    /* convert the Java pValue object to a CK-type pValue pointer */
    ckAttribute.pValue = jObjectToPrimitiveCKObjectPtr(env, jPValue, &(ckAttribute.ulValueLen));

    TRACE0("\nDEBUG: jAttributeToCKAttribute FINISHED\n");

    return ckAttribute ;
}

void masterKeyDeriveParamToCKMasterKeyDeriveParam(JNIEnv *env, jobject jParam,
        jclass masterKeyDeriveParamClass,
        CK_VERSION_PTR* cKMasterKeyDeriveParamVersion,
        CK_SSL3_RANDOM_DATA* cKMasterKeyDeriveParamRandomInfo) {
    jfieldID fieldID;
    jclass jSsl3RandomDataClass;
    jobject jRandomInfo, jRIClientRandom, jRIServerRandom, jVersion;

    // retrieve java values
    fieldID = (*env)->GetFieldID(env, masterKeyDeriveParamClass, "RandomInfo",
            "Lsun/security/pkcs11/wrapper/CK_SSL3_RANDOM_DATA;");
    if (fieldID == NULL) { return; }
    jRandomInfo = (*env)->GetObjectField(env, jParam, fieldID);
    jSsl3RandomDataClass = (*env)->FindClass(env, CLASS_SSL3_RANDOM_DATA);
    if (jSsl3RandomDataClass == NULL) { return; }
    fieldID = (*env)->GetFieldID(env, jSsl3RandomDataClass, "pClientRandom", "[B");
    if (fieldID == NULL) { return; }
    jRIClientRandom = (*env)->GetObjectField(env, jRandomInfo, fieldID);
    fieldID = (*env)->GetFieldID(env, jSsl3RandomDataClass, "pServerRandom", "[B");
    if (fieldID == NULL) { return; }
    jRIServerRandom = (*env)->GetObjectField(env, jRandomInfo, fieldID);
    fieldID = (*env)->GetFieldID(env, masterKeyDeriveParamClass, "pVersion",
            "Lsun/security/pkcs11/wrapper/CK_VERSION;");
    if (fieldID == NULL) { return; }
    jVersion = (*env)->GetObjectField(env, jParam, fieldID);

    // populate using java values
    *cKMasterKeyDeriveParamVersion = jVersionToCKVersionPtr(env, jVersion);
    if ((*env)->ExceptionCheck(env)) { return; }
    jByteArrayToCKByteArray(env, jRIClientRandom,
            &(cKMasterKeyDeriveParamRandomInfo->pClientRandom),
            &(cKMasterKeyDeriveParamRandomInfo->ulClientRandomLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jRIServerRandom,
            &(cKMasterKeyDeriveParamRandomInfo->pServerRandom),
            &(cKMasterKeyDeriveParamRandomInfo->ulServerRandomLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    return;
cleanup:
    free(*cKMasterKeyDeriveParamVersion);
    free(cKMasterKeyDeriveParamRandomInfo->pClientRandom);
    cKMasterKeyDeriveParamRandomInfo->ulClientRandomLen = 0L;
    free(cKMasterKeyDeriveParamRandomInfo->pServerRandom);
    cKMasterKeyDeriveParamRandomInfo->ulServerRandomLen = 0L;
    // explicitly set to NULL to ensure no double free possible
    *cKMasterKeyDeriveParamVersion = NULL;
    cKMasterKeyDeriveParamRandomInfo->pClientRandom = NULL;
    cKMasterKeyDeriveParamRandomInfo->pServerRandom = NULL;
}

/*
 * converts the Java CK_SSL3_MASTER_KEY_DERIVE_PARAMS object to a
 * CK_SSL3_MASTER_KEY_DERIVE_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_SSL3_MASTER_KEY_DERIVE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_SSL3_MASTER_KEY_DERIVE_PARAMS structure
 */
CK_SSL3_MASTER_KEY_DERIVE_PARAMS_PTR
jSsl3MasterKeyDeriveParamToCKSsl3MasterKeyDeriveParamPtr(JNIEnv *env,
        jobject jParam, CK_ULONG *pLength)
{
    CK_SSL3_MASTER_KEY_DERIVE_PARAMS_PTR ckParamPtr;
    jclass jSsl3MasterKeyDeriveParamsClass;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // allocate memory for CK_SSL3_MASTER_KEY_DERIVE_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_SSL3_MASTER_KEY_DERIVE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // retrieve and populate using java values
    jSsl3MasterKeyDeriveParamsClass =
            (*env)->FindClass(env, CLASS_SSL3_MASTER_KEY_DERIVE_PARAMS);
    if (jSsl3MasterKeyDeriveParamsClass == NULL) {
        goto cleanup;
    }
    masterKeyDeriveParamToCKMasterKeyDeriveParam(env, jParam,
            jSsl3MasterKeyDeriveParamsClass,
            &(ckParamPtr->pVersion), &(ckParamPtr->RandomInfo));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_SSL3_MASTER_KEY_DERIVE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_TLS12_MASTER_KEY_DERIVE_PARAMS object to a
 * CK_TLS12_MASTER_KEY_DERIVE_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_TLS12_MASTER_KEY_DERIVE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_TLS12_MASTER_KEY_DERIVE_PARAMS structure
 */
CK_TLS12_MASTER_KEY_DERIVE_PARAMS_PTR
jTls12MasterKeyDeriveParamToCKTls12MasterKeyDeriveParamPtr(JNIEnv *env,
        jobject jParam, CK_ULONG *pLength)
{
    CK_TLS12_MASTER_KEY_DERIVE_PARAMS_PTR ckParamPtr;
    jclass jTls12MasterKeyDeriveParamsClass;
    jfieldID fieldID;
    jlong prfHashMechanism;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    jTls12MasterKeyDeriveParamsClass =
        (*env)->FindClass(env, CLASS_TLS12_MASTER_KEY_DERIVE_PARAMS);
    if (jTls12MasterKeyDeriveParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env,
            jTls12MasterKeyDeriveParamsClass, "prfHashMechanism", "J");
    if (fieldID == NULL) { return NULL; }
    prfHashMechanism = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_TLS12_MASTER_KEY_DERIVE_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_TLS12_MASTER_KEY_DERIVE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    masterKeyDeriveParamToCKMasterKeyDeriveParam(env, jParam,
            jTls12MasterKeyDeriveParamsClass, &ckParamPtr->pVersion,
            &ckParamPtr->RandomInfo);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    ckParamPtr->prfHashMechanism = (CK_MECHANISM_TYPE) prfHashMechanism;

    if (pLength != NULL) {
        *pLength = sizeof(CK_TLS12_MASTER_KEY_DERIVE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_TLS_PRF_PARAMS object to a CK_TLS_PRF_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_TLS_PRF_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_TLS_PRF_PARAMS structure
 */
CK_TLS_PRF_PARAMS_PTR
jTlsPrfParamsToCKTlsPrfParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_TLS_PRF_PARAMS_PTR ckParamPtr;
    jclass jTlsPrfParamsClass;
    jfieldID fieldID;
    jobject jSeed, jLabel, jOutput;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jTlsPrfParamsClass = (*env)->FindClass(env, CLASS_TLS_PRF_PARAMS);
    if (jTlsPrfParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jTlsPrfParamsClass, "pSeed", "[B");
    if (fieldID == NULL) { return NULL; }
    jSeed = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jTlsPrfParamsClass, "pLabel", "[B");
    if (fieldID == NULL) { return NULL; }
    jLabel = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jTlsPrfParamsClass, "pOutput", "[B");
    if (fieldID == NULL) { return NULL; }
    jOutput = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_TLS_PRF_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_TLS_PRF_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    jByteArrayToCKByteArray(env, jSeed, &(ckParamPtr->pSeed), &(ckParamPtr->ulSeedLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jLabel, &(ckParamPtr->pLabel), &(ckParamPtr->ulLabelLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    ckParamPtr->pulOutputLen = calloc(1, sizeof(CK_ULONG));
    if (ckParamPtr->pulOutputLen == NULL) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jOutput, &(ckParamPtr->pOutput), ckParamPtr->pulOutputLen);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_TLS_PRF_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pSeed);
    free(ckParamPtr->pLabel);
    free(ckParamPtr->pOutput);
    free(ckParamPtr->pulOutputLen);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_TLS_MAC_PARAMS object to a CK_TLS_MAC_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_TLS_MAC_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_TLS_MAC_PARAMS structure
 */

CK_TLS_MAC_PARAMS_PTR
jTlsMacParamsToCKTlsMacParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_TLS_MAC_PARAMS_PTR ckParamPtr;
    jclass jTlsMacParamsClass;
    jfieldID fieldID;
    jlong jPrfMechanism, jUlMacLength, jUlServerOrClient;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    jTlsMacParamsClass = (*env)->FindClass(env, CLASS_TLS_MAC_PARAMS);
    if (jTlsMacParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jTlsMacParamsClass, "prfMechanism", "J");
    if (fieldID == NULL) { return NULL; }
    jPrfMechanism = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jTlsMacParamsClass, "ulMacLength", "J");
    if (fieldID == NULL) { return NULL; }
    jUlMacLength = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jTlsMacParamsClass, "ulServerOrClient", "J");
    if (fieldID == NULL) { return NULL; }
    jUlServerOrClient = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_TLS_MAC_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_TLS_MAC_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->prfHashMechanism = jLongToCKULong(jPrfMechanism);
    ckParamPtr->ulMacLength = jLongToCKULong(jUlMacLength);
    ckParamPtr->ulServerOrClient = jLongToCKULong(jUlServerOrClient);

    if (pLength != NULL) {
        *pLength = sizeof(CK_TLS_MAC_PARAMS);
    }
    return ckParamPtr;
}

/*
 * converts a Java CK_HKDF_PARAMS object to a CK_HKDF_PARAMS pointer
 *
 * @param env - used to call JNI functions to get Java classes and objects
 * @param jParam - a Java CK_HKDF_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to a new CK_HKDF_PARAMS structure
 */
CK_HKDF_PARAMS_PTR
jHkdfParamsToCKHkdfParamsPtr(JNIEnv *env, jobject jParam, CK_ULONG* pLength)
{
    CK_HKDF_PARAMS_PTR ckParamPtr = NULL;
    jclass jHkdfParamsClass;
    jfieldID fieldID;
    jboolean jbExtract, jbExpand;
    jlong jPrfHashMechanism, julSaltType, jhSaltKey;
    jobject jpSalt, jpInfo;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    jHkdfParamsClass = (*env)->FindClass(env, CLASS_HKDF_PARAMS);
    if (jHkdfParamsClass == NULL) {
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "bExtract", "Z");
    if (fieldID == NULL) {
        return NULL;
    }
    jbExtract = (*env)->GetBooleanField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "bExpand", "Z");
    if (fieldID == NULL) {
        return NULL;
    }
    jbExpand = (*env)->GetBooleanField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "prfHashMechanism", "J");
    if (fieldID == NULL) {
        return NULL;
    }
    jPrfHashMechanism = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "ulSaltType", "J");
    if (fieldID == NULL) {
        return NULL;
    }
    julSaltType = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "pSalt", "[B");
    if (fieldID == NULL) {
        return NULL;
    }
    jpSalt = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "hSaltKey", "J");
    if (fieldID == NULL) {
        return NULL;
    }
    jhSaltKey = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jHkdfParamsClass, "pInfo", "[B");
    if (fieldID == NULL) {
        return NULL;
    }
    jpInfo = (*env)->GetObjectField(env, jParam, fieldID);

    ckParamPtr = calloc(1, sizeof(CK_HKDF_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    ckParamPtr->bExtract = jBooleanToCKBBool(jbExtract);
    ckParamPtr->bExpand = jBooleanToCKBBool(jbExpand);
    ckParamPtr->prfHashMechanism = jLongToCKULong(jPrfHashMechanism);
    ckParamPtr->ulSaltType = jLongToCKULong(julSaltType);
    jByteArrayToCKByteArray(env, jpSalt, &(ckParamPtr->pSalt), &(ckParamPtr->ulSaltLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    ckParamPtr->hSaltKey = jLongToCKULong(jhSaltKey);
    jByteArrayToCKByteArray(env, jpInfo, &(ckParamPtr->pInfo), &(ckParamPtr->ulInfoLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_HKDF_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pInfo);
    free(ckParamPtr->pSalt);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts a Java CK_KEY_DERIVATION_STRING_DATA object to a CK_KEY_DERIVATION_STRING_DATA pointer
 *
 * @param env - used to call JNI functions to get Java classes and objects
 * @param jParam - a Java CK_KEY_DERIVATION_STRING_DATA object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to a new CK_KEY_DERIVATION_STRING_DATA structure
 */
CK_KEY_DERIVATION_STRING_DATA_PTR
jKeyDerivationStringDataToCKKeyDerivationStringDataPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_KEY_DERIVATION_STRING_DATA_PTR ckParamPtr = NULL;
    jclass jKeyDerivationStringDataClass;
    jfieldID fieldID;
    jobject jpData;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    jKeyDerivationStringDataClass = (*env)->FindClass(env, CLASS_KEY_DERIVATION_STRING_DATA);
    if (jKeyDerivationStringDataClass == NULL) {
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jKeyDerivationStringDataClass, "pData", "[B");
    if (fieldID == NULL) {
        return NULL;
    }
    jpData = (*env)->GetObjectField(env, jParam, fieldID);

    ckParamPtr = calloc(1, sizeof(CK_KEY_DERIVATION_STRING_DATA));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    jByteArrayToCKByteArray(env, jpData, &(ckParamPtr->pData), &(ckParamPtr->ulLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_KEY_DERIVATION_STRING_DATA);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pData);
    free(ckParamPtr);
    return NULL;
}

void keyMatParamToCKKeyMatParam(JNIEnv *env, jobject jParam,
        jclass jKeyMatParamClass,
        CK_ULONG* cKKeyMatParamUlMacSizeInBits,
        CK_ULONG* cKKeyMatParamUlKeySizeInBits,
        CK_ULONG* cKKeyMatParamUlIVSizeInBits,
        CK_BBOOL* cKKeyMatParamBIsExport,
        CK_SSL3_RANDOM_DATA* cKKeyMatParamRandomInfo,
        CK_SSL3_KEY_MAT_OUT_PTR* cKKeyMatParamPReturnedKeyMaterial)
{
    jclass jSsl3RandomDataClass, jSsl3KeyMatOutClass;
    jfieldID fieldID;
    jlong jMacSizeInBits, jKeySizeInBits, jIVSizeInBits;
    jboolean jIsExport;
    jobject jRandomInfo, jRIClientRandom, jRIServerRandom;
    jobject jReturnedKeyMaterial, jRMIvClient, jRMIvServer;
    CK_ULONG ckTemp;

    // the pointer arguments should already be initialized by caller

    // retrieve java values
    fieldID = (*env)->GetFieldID(env, jKeyMatParamClass, "ulMacSizeInBits", "J");
    if (fieldID == NULL) { return; }
    jMacSizeInBits = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jKeyMatParamClass, "ulKeySizeInBits", "J");
    if (fieldID == NULL) { return; }
    jKeySizeInBits = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jKeyMatParamClass, "ulIVSizeInBits", "J");
    if (fieldID == NULL) { return; }
    jIVSizeInBits = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jKeyMatParamClass, "bIsExport", "Z");
    if (fieldID == NULL) { return; }
    jIsExport = (*env)->GetBooleanField(env, jParam, fieldID);
    jSsl3RandomDataClass = (*env)->FindClass(env, CLASS_SSL3_RANDOM_DATA);
    if (jSsl3RandomDataClass == NULL) { return; }
    fieldID = (*env)->GetFieldID(env, jKeyMatParamClass, "RandomInfo",
            "Lsun/security/pkcs11/wrapper/CK_SSL3_RANDOM_DATA;");
    if (fieldID == NULL) { return; }
    jRandomInfo = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jSsl3RandomDataClass, "pClientRandom", "[B");
    if (fieldID == NULL) { return; }
    jRIClientRandom = (*env)->GetObjectField(env, jRandomInfo, fieldID);
    fieldID = (*env)->GetFieldID(env, jSsl3RandomDataClass, "pServerRandom", "[B");
    if (fieldID == NULL) { return; }
    jRIServerRandom = (*env)->GetObjectField(env, jRandomInfo, fieldID);
    jSsl3KeyMatOutClass = (*env)->FindClass(env, CLASS_SSL3_KEY_MAT_OUT);
    if (jSsl3KeyMatOutClass == NULL) { return; }
    fieldID = (*env)->GetFieldID(env, jKeyMatParamClass, "pReturnedKeyMaterial",
            "Lsun/security/pkcs11/wrapper/CK_SSL3_KEY_MAT_OUT;");
    if (fieldID == NULL) { return; }
    jReturnedKeyMaterial = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jSsl3KeyMatOutClass, "pIVClient", "[B");
    if (fieldID == NULL) { return; }
    jRMIvClient = (*env)->GetObjectField(env, jReturnedKeyMaterial, fieldID);
    fieldID = (*env)->GetFieldID(env, jSsl3KeyMatOutClass, "pIVServer", "[B");
    if (fieldID == NULL) { return; }
    jRMIvServer = (*env)->GetObjectField(env, jReturnedKeyMaterial, fieldID);

    // populate the specified pointers using java values
    *cKKeyMatParamUlMacSizeInBits = jLongToCKULong(jMacSizeInBits);
    *cKKeyMatParamUlKeySizeInBits = jLongToCKULong(jKeySizeInBits);
    *cKKeyMatParamUlIVSizeInBits = jLongToCKULong(jIVSizeInBits);
    *cKKeyMatParamBIsExport = jBooleanToCKBBool(jIsExport);
    jByteArrayToCKByteArray(env, jRIClientRandom,
            &(cKKeyMatParamRandomInfo->pClientRandom),
            &(cKKeyMatParamRandomInfo->ulClientRandomLen));
    if ((*env)->ExceptionCheck(env)) {
        // just return as no memory allocation yet
        return;
    }
    jByteArrayToCKByteArray(env, jRIServerRandom,
            &(cKKeyMatParamRandomInfo->pServerRandom),
            &(cKKeyMatParamRandomInfo->ulServerRandomLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    /* allocate memory for pReturnedKeyMaterial */
    *cKKeyMatParamPReturnedKeyMaterial =
            (CK_SSL3_KEY_MAT_OUT_PTR) calloc(1, sizeof(CK_SSL3_KEY_MAT_OUT));
    if (*cKKeyMatParamPReturnedKeyMaterial == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        goto cleanup;
    }

    // the handles are output params only, no need to fetch them from Java
    (*cKKeyMatParamPReturnedKeyMaterial)->hClientMacSecret = 0;
    (*cKKeyMatParamPReturnedKeyMaterial)->hServerMacSecret = 0;
    (*cKKeyMatParamPReturnedKeyMaterial)->hClientKey = 0;
    (*cKKeyMatParamPReturnedKeyMaterial)->hServerKey = 0;

    jByteArrayToCKByteArray(env, jRMIvClient,
            &((*cKKeyMatParamPReturnedKeyMaterial)->pIVClient), &ckTemp);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jRMIvServer,
            &((*cKKeyMatParamPReturnedKeyMaterial)->pIVServer), &ckTemp);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    return;
cleanup:
    free(cKKeyMatParamRandomInfo->pClientRandom);
    free(cKKeyMatParamRandomInfo->pServerRandom);
    if ((*cKKeyMatParamPReturnedKeyMaterial) != NULL) {
        free((*cKKeyMatParamPReturnedKeyMaterial)->pIVClient);
        free(*cKKeyMatParamPReturnedKeyMaterial);
    }
    // explicitly set to NULL to ensure no double free possible
    cKKeyMatParamRandomInfo->pClientRandom = NULL;
    cKKeyMatParamRandomInfo->pServerRandom = NULL;
    *cKKeyMatParamPReturnedKeyMaterial = NULL;
    return;
}
/*
 * converts the Java CK_SSL3_KEY_MAT_PARAMS object to a
 * CK_SSL3_KEY_MAT_PARAMS pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_SSL3_KEY_MAT_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_SSL3_KEY_MAT_PARAMS structure
 */
CK_SSL3_KEY_MAT_PARAMS_PTR
jSsl3KeyMatParamToCKSsl3KeyMatParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_SSL3_KEY_MAT_PARAMS_PTR ckParamPtr;
    jclass jSsl3KeyMatParamsClass;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // allocate memory for CK_SSL3_KEY_MAT_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_SSL3_KEY_MAT_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // retrieve and  populate using java values
    jSsl3KeyMatParamsClass = (*env)->FindClass(env,
            CLASS_SSL3_KEY_MAT_PARAMS);
    if (jSsl3KeyMatParamsClass == NULL) {
        goto cleanup;
    }
    keyMatParamToCKKeyMatParam(env, jParam, jSsl3KeyMatParamsClass,
            &(ckParamPtr->ulMacSizeInBits), &(ckParamPtr->ulKeySizeInBits),
            &(ckParamPtr->ulIVSizeInBits), &(ckParamPtr->bIsExport),
            &(ckParamPtr->RandomInfo), &(ckParamPtr->pReturnedKeyMaterial));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_SSL3_KEY_MAT_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_TLS12_KEY_MAT_PARAMS object to a
 * CK_TLS12_KEY_MAT_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_TLS12_KEY_MAT_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_TLS12_KEY_MAT_PARAMS structure
 */
CK_TLS12_KEY_MAT_PARAMS_PTR jTls12KeyMatParamToCKTls12KeyMatParamPtr(JNIEnv *env,
        jobject jParam, CK_ULONG *pLength)
{
    CK_TLS12_KEY_MAT_PARAMS_PTR ckParamPtr;
    jclass jTls12KeyMatParamsClass;
    jfieldID fieldID;
    jlong prfHashMechanism;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jTls12KeyMatParamsClass = (*env)->FindClass(env,
            CLASS_TLS12_KEY_MAT_PARAMS);
    if (jTls12KeyMatParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jTls12KeyMatParamsClass,
            "prfHashMechanism", "J");
    if (fieldID == NULL) { return NULL; }
    prfHashMechanism = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_TLS12_KEY_MAT_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_TLS12_KEY_MAT_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    keyMatParamToCKKeyMatParam(env, jParam, jTls12KeyMatParamsClass,
            &(ckParamPtr->ulMacSizeInBits), &(ckParamPtr->ulKeySizeInBits),
            &(ckParamPtr->ulIVSizeInBits), &(ckParamPtr->bIsExport),
            &(ckParamPtr->RandomInfo), &(ckParamPtr->pReturnedKeyMaterial));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    ckParamPtr->prfHashMechanism = (CK_MECHANISM_TYPE)prfHashMechanism;

    if (pLength != NULL) {
        *pLength = sizeof(CK_TLS12_KEY_MAT_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_AES_CTR_PARAMS object to a CK_AES_CTR_PARAMS pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_AES_CTR_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_AES_CTR_PARAMS structure
 */
CK_AES_CTR_PARAMS_PTR
jAesCtrParamsToCKAesCtrParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_AES_CTR_PARAMS_PTR ckParamPtr;
    jclass jAesCtrParamsClass;
    jfieldID fieldID;
    jlong jCounterBits;
    jobject jCb;
    CK_BYTE_PTR ckBytes = NULL;
    CK_ULONG ckTemp;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    jAesCtrParamsClass = (*env)->FindClass(env, CLASS_AES_CTR_PARAMS);
    if (jAesCtrParamsClass == NULL) { return NULL; }
    if (!(*env)->IsInstanceOf(env, jParam, jAesCtrParamsClass)) {
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jAesCtrParamsClass, "ulCounterBits", "J");
    if (fieldID == NULL) { return NULL; }
    jCounterBits = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jAesCtrParamsClass, "cb", "[B");
    if (fieldID == NULL) { return NULL; }
    jCb = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_AES_CTR_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_AES_CTR_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    jByteArrayToCKByteArray(env, jCb, &ckBytes, &ckTemp);
    if ((*env)->ExceptionCheck(env) || ckTemp != 16) {
        goto cleanup;
    }
    memcpy(ckParamPtr->cb, ckBytes, ckTemp);
    free(ckBytes);

    ckParamPtr->ulCounterBits = jLongToCKULong(jCounterBits);

    if (pLength != NULL) {
        *pLength = sizeof(CK_AES_CTR_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckBytes);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_GCM_PARAMS object to a CK_GCM_PARAMS pointer
 * Note: Early NSS versions crash w/ CK_GCM_PARAMS and need to use
 * CK_GCM_PARAMS_NO_IVBITS to avoid SIGSEGV.
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_GCM_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_GCM_PARAMS structure
 */
CK_GCM_PARAMS_PTR
jGCMParamsToCKGCMParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_GCM_PARAMS_PTR ckParamPtr;
    jclass jGcmParamsClass;
    jfieldID fieldID;
    jobject jIv, jAad;
    jlong jTagLen;

    TRACE0("DEBUG jGCMParamsToCKGCMParam is called\n");

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    jGcmParamsClass = (*env)->FindClass(env, CLASS_GCM_PARAMS);
    if (jGcmParamsClass == NULL) { return NULL; }
    if (!(*env)->IsInstanceOf(env, jParam, jGcmParamsClass)) {
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jGcmParamsClass, "iv", "[B");
    if (fieldID == NULL) { return NULL; }
    jIv = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jGcmParamsClass, "aad", "[B");
    if (fieldID == NULL) { return NULL; }
    jAad = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jGcmParamsClass, "tagBits", "J");
    if (fieldID == NULL) { return NULL; }
    jTagLen = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_GCM_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_GCM_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    jByteArrayToCKByteArray(env, jIv, &(ckParamPtr->pIv), &(ckParamPtr->ulIvLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    // adjust since the value is in bits
    ckParamPtr->ulIvBits = ckParamPtr->ulIvLen << 3;

    jByteArrayToCKByteArray(env, jAad, &(ckParamPtr->pAAD), &(ckParamPtr->ulAADLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    ckParamPtr->ulTagBits = jLongToCKULong(jTagLen);

    if (pLength != NULL) {
        *pLength = sizeof(CK_GCM_PARAMS);
    }
    TRACE1("Created inner GCM_PARAMS PTR %p\n", ckParamPtr);
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pIv);
    free(ckParamPtr->pAAD);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_CCM_PARAMS object to a CK_CCM_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_CCM_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_CCM_PARAMS structure
 */
CK_CCM_PARAMS_PTR
jCCMParamsToCKCCMParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_CCM_PARAMS_PTR ckParamPtr;
    jclass jCcmParamsClass;
    jfieldID fieldID;
    jobject jNonce, jAad;
    jlong jDataLen, jMacLen;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jCcmParamsClass = (*env)->FindClass(env, CLASS_CCM_PARAMS);
    if (jCcmParamsClass == NULL) { return NULL; }
    if (!(*env)->IsInstanceOf(env, jParam, jCcmParamsClass)) {
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jCcmParamsClass, "dataLen", "J");
    if (fieldID == NULL) { return NULL; }
    jDataLen = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jCcmParamsClass, "nonce", "[B");
    if (fieldID == NULL) { return NULL; }
    jNonce = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jCcmParamsClass, "aad", "[B");
    if (fieldID == NULL) { return NULL; }
    jAad = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jCcmParamsClass, "macLen", "J");
    if (fieldID == NULL) { return NULL; }
    jMacLen = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_CCM_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_CCM_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->ulDataLen = jLongToCKULong(jDataLen);
    jByteArrayToCKByteArray(env, jNonce, &(ckParamPtr->pNonce),
            &(ckParamPtr->ulNonceLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    jByteArrayToCKByteArray(env, jAad, &(ckParamPtr->pAAD),
            &(ckParamPtr->ulAADLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    ckParamPtr->ulMACLen = jLongToCKULong(jMacLen);

    if (pLength != NULL) {
        *pLength = sizeof(CK_CCM_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pNonce);
    free(ckParamPtr->pAAD);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_SALSA20_CHACHA20_POLY1305_PARAMS object to a
 * CK_SALSA20_CHACHA20_POLY1305_PARAMS pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java CK_SALSA20_CHACHA20_POLY1305_PARAMS object to
 *         convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_SALSA20_CHACHA20_POLY1305_PARAMS structure
 */
CK_SALSA20_CHACHA20_POLY1305_PARAMS_PTR
jSalsaChaChaPolyParamsToCKSalsaChaChaPolyParamPtr(
        JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_SALSA20_CHACHA20_POLY1305_PARAMS_PTR ckParamPtr;
    jclass jParamsClass;
    jfieldID fieldID;
    jobject jNonce, jAad;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jParamsClass = (*env)->FindClass(env,
            CLASS_SALSA20_CHACHA20_POLY1305_PARAMS);
    if (jParamsClass == NULL) { return NULL; }
    if (!(*env)->IsInstanceOf(env, jParam, jParamsClass)) {
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jParamsClass, "nonce", "[B");
    if (fieldID == NULL) { return NULL; }
    jNonce = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jParamsClass, "aad", "[B");
    if (fieldID == NULL) { return NULL; }
    jAad = (*env)->GetObjectField(env, jParam, fieldID);
    // allocate memory for CK_SALSA20_CHACHA20_POLY1305_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_SALSA20_CHACHA20_POLY1305_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    jByteArrayToCKByteArray(env, jNonce, &(ckParamPtr->pNonce),
            &(ckParamPtr->ulNonceLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    jByteArrayToCKByteArray(env, jAad, &(ckParamPtr->pAAD),
            &(ckParamPtr->ulAADLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_SALSA20_CHACHA20_POLY1305_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pNonce);
    free(ckParamPtr->pAAD);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts a Java CK_MECHANISM object into a CK_MECHANISM pointer
 * pointer.
 *
 * @param env - used to call JNI funktions to get the values out of the Java object
 * @param jMech - the Java CK_MECHANISM object to convert
 * @return pointer to the new CK_MECHANISM structure
 */
CK_MECHANISM_PTR jMechanismToCKMechanismPtr(JNIEnv *env, jobject jMech)
{
    CK_MECHANISM_PTR ckpMech;
    jlong jMechType = (*env)->GetLongField(env, jMech, mech_mechanismID);
    jobject jParam = (*env)->GetObjectField(env, jMech, mech_pParameterID);

    /* allocate memory for CK_MECHANISM_PTR */
    ckpMech =  (CK_MECHANISM_PTR) calloc(1, sizeof(CK_MECHANISM));
    if (ckpMech == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }
    TRACE1("DEBUG jMechanismToCKMechanismPtr: allocated mech %p\n", ckpMech);

    ckpMech->mechanism = jLongToCKULong(jMechType);

    /* convert the specific Java mechanism parameter object to a pointer to a
     *  CK-type mechanism structure
     */
    if (jParam == NULL) {
        ckpMech->pParameter = NULL;
        ckpMech->ulParameterLen = 0;
    } else {
        ckpMech->pParameter = jMechParamToCKMechParamPtr(env, jParam,
            ckpMech->mechanism, &(ckpMech->ulParameterLen));
    }
    return ckpMech;
}

/*
 * converts the pValue of a CK_ATTRIBUTE structure into a Java Object by
 * checking the type of the attribute. A PKCS#11 attribute value can
 * be a CK_ULONG, CK_BYTE[], CK_CHAR[], big integer, CK_BBOOL, CK_UTF8CHAR[],
 * CK_DATE or CK_FLAGS that gets converted to a corresponding Java object.
 *
 * @param env - used to call JNI functions to create the new Java object
 * @param ckpAttribute - the pointer to the CK_ATTRIBUTE structure that contains the type
 *                       and the pValue to convert
 * @return the new Java object of the CK-type pValue
 */
jobject ckAttributeValueToJObject(JNIEnv *env, const CK_ATTRIBUTE_PTR ckpAttribute)
{
    jint jValueLength;
    jobject jValueObject = NULL;

    jValueLength = ckULongToJInt(ckpAttribute->ulValueLen);

    if ((jValueLength <= 0) || (ckpAttribute->pValue == NULL)) {
        return NULL ;
    }

    switch(ckpAttribute->type) {
        case CKA_CLASS:
            /* value CK_OBJECT_CLASS, defacto a CK_ULONG */
        case CKA_KEY_TYPE:
            /* value CK_KEY_TYPE, defacto a CK_ULONG */
        case CKA_CERTIFICATE_TYPE:
            /* value CK_CERTIFICATE_TYPE, defacto a CK_ULONG */
        case CKA_HW_FEATURE_TYPE:
            /* value CK_HW_FEATURE_TYPE, defacto a CK_ULONG */
        case CKA_MODULUS_BITS:
        case CKA_VALUE_BITS:
        case CKA_VALUE_LEN:
        case CKA_KEY_GEN_MECHANISM:
        case CKA_PRIME_BITS:
        case CKA_SUB_PRIME_BITS:
            /* value CK_ULONG */
            jValueObject = ckULongPtrToJLongObject(env, (CK_ULONG*) ckpAttribute->pValue);
            break;

            /* can be CK_BYTE[],CK_CHAR[] or big integer; defacto always CK_BYTE[] */
        case CKA_VALUE:
        case CKA_OBJECT_ID:
        case CKA_SUBJECT:
        case CKA_ID:
        case CKA_ISSUER:
        case CKA_SERIAL_NUMBER:
        case CKA_OWNER:
        case CKA_AC_ISSUER:
        case CKA_ATTR_TYPES:
        case CKA_ECDSA_PARAMS:
            /* CKA_EC_PARAMS is the same, these two are equivalent */
        case CKA_EC_POINT:
        case CKA_PRIVATE_EXPONENT:
        case CKA_PRIME_1:
        case CKA_PRIME_2:
        case CKA_EXPONENT_1:
        case CKA_EXPONENT_2:
        case CKA_COEFFICIENT:
            /* value CK_BYTE[] */
            jValueObject = ckByteArrayToJByteArray(env, (CK_BYTE*) ckpAttribute->pValue, jValueLength);
            break;

        case CKA_RESET_ON_INIT:
        case CKA_HAS_RESET:
        case CKA_TOKEN:
        case CKA_PRIVATE:
        case CKA_MODIFIABLE:
        case CKA_DERIVE:
        case CKA_LOCAL:
        case CKA_ENCRYPT:
        case CKA_VERIFY:
        case CKA_VERIFY_RECOVER:
        case CKA_WRAP:
        case CKA_SENSITIVE:
        case CKA_SECONDARY_AUTH:
        case CKA_DECRYPT:
        case CKA_SIGN:
        case CKA_SIGN_RECOVER:
        case CKA_UNWRAP:
        case CKA_EXTRACTABLE:
        case CKA_ALWAYS_SENSITIVE:
        case CKA_NEVER_EXTRACTABLE:
        case CKA_TRUSTED:
            /* value CK_BBOOL */
            jValueObject = ckBBoolPtrToJBooleanObject(env, (CK_BBOOL*) ckpAttribute->pValue);
            break;

        case CKA_LABEL:
        case CKA_APPLICATION:
            /* value RFC 2279 (UTF-8) string */
            jValueObject = ckUTF8CharArrayToJCharArray(env, (CK_UTF8CHAR*) ckpAttribute->pValue, jValueLength);
            break;

        case CKA_START_DATE:
        case CKA_END_DATE:
            /* value CK_DATE */
            jValueObject = ckDatePtrToJDateObject(env, (CK_DATE*) ckpAttribute->pValue);
            break;

        case CKA_MODULUS:
        case CKA_PUBLIC_EXPONENT:
        case CKA_PRIME:
        case CKA_SUBPRIME:
        case CKA_BASE:
            /* value big integer, i.e. CK_BYTE[] */
            jValueObject = ckByteArrayToJByteArray(env, (CK_BYTE*) ckpAttribute->pValue, jValueLength);
            break;

        case CKA_AUTH_PIN_FLAGS:
            jValueObject = ckULongPtrToJLongObject(env, (CK_ULONG*) ckpAttribute->pValue);
            /* value FLAGS, defacto a CK_ULONG */
            break;

        case CKA_VENDOR_DEFINED:
            /* we make a CK_BYTE[] out of this */
            jValueObject = ckByteArrayToJByteArray(env, (CK_BYTE*) ckpAttribute->pValue, jValueLength);
            break;

        // Netscape trust attributes
        case CKA_NETSCAPE_TRUST_SERVER_AUTH:
        case CKA_NETSCAPE_TRUST_CLIENT_AUTH:
        case CKA_NETSCAPE_TRUST_CODE_SIGNING:
        case CKA_NETSCAPE_TRUST_EMAIL_PROTECTION:
            /* value CK_ULONG */
            jValueObject = ckULongPtrToJLongObject(env, (CK_ULONG*) ckpAttribute->pValue);
            break;

        default:
            /* we make a CK_BYTE[] out of this */
            jValueObject = ckByteArrayToJByteArray(env, (CK_BYTE*) ckpAttribute->pValue, jValueLength);
            break;
    }

    return jValueObject ;
}

/*
 * the following functions convert a Java mechanism parameter object to a PKCS#11
 * mechanism parameter structure
 *
 * CK_<Param>_PARAMS j<Param>ParamToCK<Param>Param(JNIEnv *env,
 *                                                 jobject jParam);
 *
 * These functions get a Java object, that must be the right Java mechanism
 * object and they return the new PKCS#11 mechanism parameter structure.
 * Every field of the Java object is retrieved, gets converted to a corresponding
 * PKCS#11 type and is set in the new PKCS#11 structure.
 */

/*
 * converts the given Java mechanism parameter to a CK mechanism parameter
 * pointer and store the length in bytes in the length variable.
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java mechanism parameter object to convert
 * @param ckMech - the PKCS#11 mechanism type
 * @param ckpLength - the reference of the length in bytes of the new CK mechanism parameter
 *                    structure
 * @return pointer to the new CK mechanism parameter structure
 */
CK_VOID_PTR jMechParamToCKMechParamPtr(JNIEnv *env, jobject jParam,
        CK_MECHANISM_TYPE ckMech, CK_ULONG *ckpLength)
{
    CK_VOID_PTR ckpParamPtr;
    if (jParam == NULL) {
        ckpParamPtr = NULL;
        *ckpLength = 0;
    } else if ((*env)->IsInstanceOf(env, jParam, jByteArrayClass)) {
        jByteArrayToCKByteArray(env, jParam, (CK_BYTE_PTR *) &ckpParamPtr, ckpLength);
    } else if ((*env)->IsInstanceOf(env, jParam, jLongClass)) {
        ckpParamPtr = jLongObjectToCKULongPtr(env, jParam);
        *ckpLength = sizeof(CK_ULONG);
    } else {
        ckpParamPtr = jMechParamToCKMechParamPtrSlow(env, jParam, ckMech, ckpLength);
    }
    return ckpParamPtr;
}

CK_VOID_PTR jMechParamToCKMechParamPtrSlow(JNIEnv *env, jobject jParam,
        CK_MECHANISM_TYPE ckMech, CK_ULONG *ckpLength)
{
    CK_VOID_PTR ckpParamPtr = NULL;

    /*
     * Most common cases, i.e. NULL/byte[]/long, are already handled by
     * jMechParamToCKMechParam before calling this method.
     */
    TRACE1("\nDEBUG: jMechParamToCKMechParamPtrSlow, mech=0x%lX\n", ckMech);

    switch (ckMech) {
        case CKM_SSL3_PRE_MASTER_KEY_GEN:
        case CKM_TLS_PRE_MASTER_KEY_GEN:
            ckpParamPtr = jVersionToCKVersionPtr(env, jParam);
            if (ckpParamPtr != NULL) {
                *ckpLength = sizeof(CK_VERSION);
            } else {
                *ckpLength = 0;
            }
            break;
        case CKM_SSL3_MASTER_KEY_DERIVE:
        case CKM_TLS_MASTER_KEY_DERIVE:
        case CKM_SSL3_MASTER_KEY_DERIVE_DH:
        case CKM_TLS_MASTER_KEY_DERIVE_DH:
            ckpParamPtr = jSsl3MasterKeyDeriveParamToCKSsl3MasterKeyDeriveParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_SSL3_KEY_AND_MAC_DERIVE:
        case CKM_TLS_KEY_AND_MAC_DERIVE:
            ckpParamPtr = jSsl3KeyMatParamToCKSsl3KeyMatParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_TLS12_KEY_AND_MAC_DERIVE:
            ckpParamPtr = jTls12KeyMatParamToCKTls12KeyMatParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_TLS12_MASTER_KEY_DERIVE:
        case CKM_TLS12_MASTER_KEY_DERIVE_DH:
            ckpParamPtr = jTls12MasterKeyDeriveParamToCKTls12MasterKeyDeriveParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_TLS_PRF:
        case CKM_NSS_TLS_PRF_GENERAL:
            ckpParamPtr = jTlsPrfParamsToCKTlsPrfParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_TLS_MAC:
            ckpParamPtr = jTlsMacParamsToCKTlsMacParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_HKDF_DATA:
        case CKM_HKDF_DERIVE:
            ckpParamPtr = jHkdfParamsToCKHkdfParamsPtr(env, jParam, ckpLength);
            break;
        case CKM_CONCATENATE_BASE_AND_DATA:
        case CKM_CONCATENATE_DATA_AND_BASE:
            ckpParamPtr = jKeyDerivationStringDataToCKKeyDerivationStringDataPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_AES_CTR:
            ckpParamPtr = jAesCtrParamsToCKAesCtrParamPtr(env, jParam,
                    ckpLength);
            break;
        case CKM_AES_GCM:
            ckpParamPtr = jGCMParamsToCKGCMParamPtr(env, jParam, ckpLength);
            break;
        case CKM_AES_CCM:
            ckpParamPtr = jCCMParamsToCKCCMParamPtr(env, jParam, ckpLength);
            break;
       case CKM_CHACHA20_POLY1305:
            ckpParamPtr =
                    jSalsaChaChaPolyParamsToCKSalsaChaChaPolyParamPtr(env,
                    jParam, ckpLength);
            break;
        case CKM_RSA_PKCS_OAEP:
            ckpParamPtr = jRsaPkcsOaepParamToCKRsaPkcsOaepParamPtr(env, jParam, ckpLength);
            break;
        case CKM_PBA_SHA1_WITH_SHA1_HMAC:
        case CKM_NSS_PKCS12_PBE_SHA224_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA256_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA384_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA512_HMAC_KEY_GEN:
            ckpParamPtr = jPbeParamToCKPbeParamPtr(env, jParam, ckpLength);
            break;
        case CKM_PKCS5_PBKD2:
            ckpParamPtr = jPkcs5Pbkd2ParamToCKPkcs5Pbkd2ParamPtr(env, jParam, ckpLength);
            break;
        case CKM_RSA_PKCS_PSS:
        case CKM_SHA1_RSA_PKCS_PSS:
        case CKM_SHA256_RSA_PKCS_PSS:
        case CKM_SHA384_RSA_PKCS_PSS:
        case CKM_SHA512_RSA_PKCS_PSS:
        case CKM_SHA224_RSA_PKCS_PSS:
            ckpParamPtr = jRsaPkcsPssParamToCKRsaPkcsPssParamPtr(env, jParam, ckpLength);
            break;
        case CKM_ECDH1_DERIVE:
        case CKM_ECDH1_COFACTOR_DERIVE:
            ckpParamPtr = jEcdh1DeriveParamToCKEcdh1DeriveParamPtr(env, jParam, ckpLength);
            break;
        case CKM_ECMQV_DERIVE:
            ckpParamPtr = jEcdh2DeriveParamToCKEcdh2DeriveParamPtr(env, jParam, ckpLength);
            break;
        case CKM_X9_42_DH_DERIVE:
            ckpParamPtr = jX942Dh1DeriveParamToCKX942Dh1DeriveParamPtr(env, jParam, ckpLength);
            break;
        case CKM_X9_42_DH_HYBRID_DERIVE:
        case CKM_X9_42_MQV_DERIVE:
            ckpParamPtr = jX942Dh2DeriveParamToCKX942Dh2DeriveParamPtr(env, jParam, ckpLength);
            break;
        // defined by pkcs11.h but we don't support
        case CKM_KEA_DERIVE: // CK_KEA_DERIVE_PARAMS
        case CKM_RC2_CBC: // CK_RC2_CBC_PARAMS
        case CKM_RC2_MAC_GENERAL: // CK_RC2_MAC_GENERAL_PARAMS
        case CKM_RC5_ECB: // CK_RC5_PARAMS
        case CKM_RC5_MAC: // CK_RC5_PARAMS
        case CKM_RC5_CBC: // CK_RC5_CBC_PARAMS
        case CKM_RC5_MAC_GENERAL: // CK_RC5_MAC_GENERAL_PARAMS
        case CKM_SKIPJACK_PRIVATE_WRAP: // CK_SKIPJACK_PRIVATE_WRAP_PARAMS
        case CKM_SKIPJACK_RELAYX: // CK_SKIPJACK_RELAYX_PARAMS
        case CKM_KEY_WRAP_SET_OAEP: // CK_KEY_WRAP_SET_OAEP_PARAMS
            p11ThrowPKCS11RuntimeException(env, "No parameter support for this mechanism");
            break;
        default:
            /* if everything failed up to here */
            /* try if the parameter is a primitive Java type */
            ckpParamPtr = jObjectToPrimitiveCKObjectPtr(env, jParam, ckpLength);
            /* *ckpParamPtr = jObjectToCKVoidPtr(jParam); */
            /* *ckpLength = 1; */
    }
    TRACE0("\nDEBUG: jMechParamToCKMechParamPtrSlow FINISHED\n");

    if ((*env)->ExceptionCheck(env)) {
        return NULL;
    }

    return ckpParamPtr;
}

/*
 * converts the Java CK_RSA_PKCS_OAEP_PARAMS object to a
 * CK_RSA_PKCS_OAEP_PARAMS pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_RSA_PKCS_OAEP_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_RSA_PKCS_OAEP_PARAMS structure
 */
CK_RSA_PKCS_OAEP_PARAMS_PTR
jRsaPkcsOaepParamToCKRsaPkcsOaepParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_RSA_PKCS_OAEP_PARAMS_PTR ckParamPtr;
    jclass jRsaPkcsOaepParamsClass;
    jfieldID fieldID;
    jlong jHashAlg, jMgf, jSource;
    jobject jSourceData;

    if (pLength!= NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    jRsaPkcsOaepParamsClass = (*env)->FindClass(env, CLASS_RSA_PKCS_OAEP_PARAMS);
    if (jRsaPkcsOaepParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jRsaPkcsOaepParamsClass, "hashAlg", "J");
    if (fieldID == NULL) { return NULL; }
    jHashAlg = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jRsaPkcsOaepParamsClass, "mgf", "J");
    if (fieldID == NULL) { return NULL; }
    jMgf = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jRsaPkcsOaepParamsClass, "source", "J");
    if (fieldID == NULL) { return NULL; }
    jSource = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jRsaPkcsOaepParamsClass, "pSourceData", "[B");
    if (fieldID == NULL) { return NULL; }
    jSourceData = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_RSA_PKCS_OAEP_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_RSA_PKCS_OAEP_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->hashAlg = jLongToCKULong(jHashAlg);
    ckParamPtr->mgf = jLongToCKULong(jMgf);
    ckParamPtr->source = jLongToCKULong(jSource);
    jByteArrayToCKByteArray(env, jSourceData, (CK_BYTE_PTR*) &(ckParamPtr->pSourceData),
            &(ckParamPtr->ulSourceDataLen));
    if ((*env)->ExceptionCheck(env)) {
        free(ckParamPtr);
        return NULL;
    }

    if (pLength!= NULL) {
        *pLength = sizeof(CK_RSA_PKCS_OAEP_PARAMS);
    }
    return ckParamPtr;
}

/*
 * converts the Java CK_PBE_PARAMS object to a CK_PBE_PARAMS pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_PBE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_PBE_PARAMS structure
 */
CK_PBE_PARAMS_PTR
jPbeParamToCKPbeParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_PBE_PARAMS_PTR ckParamPtr;
    jclass jPbeParamsClass;
    jfieldID fieldID;
    jlong jIteration;
    jobject jInitVector, jPassword, jSalt;
    CK_ULONG ckTemp;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jPbeParamsClass = (*env)->FindClass(env, CLASS_PBE_PARAMS);
    if (jPbeParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jPbeParamsClass, "pInitVector", "[B");
    if (fieldID == NULL) { return NULL; }
    jInitVector = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPbeParamsClass, "pPassword", "[C");
    if (fieldID == NULL) { return NULL; }
    jPassword = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPbeParamsClass, "pSalt", "[B");
    if (fieldID == NULL) { return NULL; }
    jSalt = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPbeParamsClass, "ulIteration", "J");
    if (fieldID == NULL) { return NULL; }
    jIteration = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_PBE_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_PBE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->ulIteration = jLongToCKULong(jIteration);
    jByteArrayToCKByteArray(env, jInitVector, &(ckParamPtr->pInitVector), &ckTemp);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jCharArrayToCKUTF8CharArray(env, jPassword, &(ckParamPtr->pPassword), &(ckParamPtr->ulPasswordLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jSalt, &(ckParamPtr->pSalt), &(ckParamPtr->ulSaltLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_PBE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pInitVector);
    if (ckParamPtr->pPassword != NULL) {
        memset(ckParamPtr->pPassword, 0, ckParamPtr->ulPasswordLen);
    }
    free(ckParamPtr->pPassword);
    free(ckParamPtr->pSalt);
    free(ckParamPtr);
    return NULL;
}

/*
 * Copy back the initialization vector from the native structure to the
 * Java object. This is only used for CKM_PBE_* mechanisms and their
 * CK_PBE_PARAMS parameters.
 *
 */
void copyBackPBEInitializationVector(JNIEnv *env, CK_MECHANISM *ckMechanism, jobject jMechanism)
{
    jclass jMechanismClass, jPbeParamsClass;
    CK_PBE_PARAMS *ckParam;
    jfieldID fieldID;
    CK_MECHANISM_TYPE ckMechanismType;
    jlong jMechanismType;
    jobject jParameter;
    jobject jInitVector;
    jint jInitVectorLength;
    CK_CHAR_PTR initVector;
    int i;
    jchar* jInitVectorChars;

    /* get mechanism */
    jMechanismClass = (*env)->FindClass(env, CLASS_MECHANISM);
    if (jMechanismClass == NULL) { return; }
    fieldID = (*env)->GetFieldID(env, jMechanismClass, "mechanism", "J");
    if (fieldID == NULL) { return; }
    jMechanismType = (*env)->GetLongField(env, jMechanism, fieldID);
    ckMechanismType = jLongToCKULong(jMechanismType);
    if (ckMechanismType != ckMechanism->mechanism) {
        /* we do not have matching types, this should not occur */
        return;
    }

    jPbeParamsClass = (*env)->FindClass(env, CLASS_PBE_PARAMS);
    if (jPbeParamsClass == NULL) { return; }
    ckParam = (CK_PBE_PARAMS *) ckMechanism->pParameter;
    if (ckParam != NULL_PTR) {
        initVector = ckParam->pInitVector;
        if (initVector != NULL_PTR) {
            /* get pParameter */
            fieldID = (*env)->GetFieldID(env, jMechanismClass, "pParameter", "Ljava/lang/Object;");
            if (fieldID == NULL) { return; }
            jParameter = (*env)->GetObjectField(env, jMechanism, fieldID);
            fieldID = (*env)->GetFieldID(env, jPbeParamsClass, "pInitVektor", "[C");
            if (fieldID == NULL) { return; }
            jInitVector = (*env)->GetObjectField(env, jParameter, fieldID);

            if (jInitVector != NULL) {
                jInitVectorLength = (*env)->GetArrayLength(env, jInitVector);
                jInitVectorChars = (*env)->GetCharArrayElements(env, jInitVector, NULL);
                if (jInitVectorChars == NULL) { return; }

                /* copy the chars to the Java buffer */
                for (i=0; i < jInitVectorLength; i++) {
                    jInitVectorChars[i] = ckCharToJChar(initVector[i]);
                }
                /* copy back the Java buffer to the object */
                (*env)->ReleaseCharArrayElements(env, jInitVector, jInitVectorChars, 0);
            }
        }
    }
}

#define PBKD2_PARAM_SET(member, value)                              \
    do {                                                            \
        if(ckParamPtr->version == PARAMS) {                         \
            ckParamPtr->params.v1.member = value;                   \
        } else {                                                    \
            ckParamPtr->params.v2.member = value;                   \
        }                                                           \
    } while(0)

#define PBKD2_PARAM_ADDR(member)                                    \
    (                                                               \
        (ckParamPtr->version == PARAMS) ?                           \
        (void*) &ckParamPtr->params.v1.member :                     \
        (void*) &ckParamPtr->params.v2.member                       \
    )

/*
 * converts a Java CK_PKCS5_PBKD2_PARAMS object to a CK_PKCS5_PBKD2_PARAMS
 * pointer, or a Java CK_PKCS5_PBKD2_PARAMS2 object to a CK_PKCS5_PBKD2_PARAMS2
 * pointer
 *
 * @param env - used to call JNI functions to get the Java classes and objects
 * @param jParam - the Java object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new structure
 */
CK_VOID_PTR
jPkcs5Pbkd2ParamToCKPkcs5Pbkd2ParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    VersionedPbkd2ParamsPtr ckParamPtr;
    ParamVersion paramVersion;
    CK_ULONG_PTR pUlPasswordLen;
    jclass jPkcs5Pbkd2ParamsClass;
    jfieldID fieldID;
    jlong jSaltSource, jIteration, jPrf;
    jobject jSaltSourceData, jPrfData, jPassword;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    if ((jPkcs5Pbkd2ParamsClass =
            (*env)->FindClass(env, CLASS_PKCS5_PBKD2_PARAMS)) != NULL
            && (*env)->IsInstanceOf(env, jParam, jPkcs5Pbkd2ParamsClass)) {
        paramVersion = PARAMS;
    } else if ((jPkcs5Pbkd2ParamsClass =
            (*env)->FindClass(env, CLASS_PKCS5_PBKD2_PARAMS2)) != NULL
            && (*env)->IsInstanceOf(env, jParam, jPkcs5Pbkd2ParamsClass)) {
        paramVersion = PARAMS2;
    } else {
        p11ThrowPKCS11RuntimeException(env, "Unknown PBKD2 mechanism parameters class.");
        return NULL;
    }
    fieldID = (*env)->GetFieldID(env, jPkcs5Pbkd2ParamsClass, "saltSource", "J");
    if (fieldID == NULL) { return NULL; }
    jSaltSource = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPkcs5Pbkd2ParamsClass, "pSaltSourceData", "[B");
    if (fieldID == NULL) { return NULL; }
    jSaltSourceData = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPkcs5Pbkd2ParamsClass, "iterations", "J");
    if (fieldID == NULL) { return NULL; }
    jIteration = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPkcs5Pbkd2ParamsClass, "prf", "J");
    if (fieldID == NULL) { return NULL; }
    jPrf = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPkcs5Pbkd2ParamsClass, "pPrfData", "[B");
    if (fieldID == NULL) { return NULL; }
    jPrfData = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jPkcs5Pbkd2ParamsClass, "pPassword", "[C");
    if (fieldID == NULL) { return NULL; }
    jPassword = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for VersionedPbkd2Params and store the structure version
    ckParamPtr = calloc(1, sizeof(VersionedPbkd2Params));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }
    ckParamPtr->version = paramVersion;

    // populate using java values
    PBKD2_PARAM_SET(saltSource, jLongToCKULong(jSaltSource));
    jByteArrayToCKByteArray(env, jSaltSourceData,
            (CK_BYTE_PTR *) PBKD2_PARAM_ADDR(pSaltSourceData),
            PBKD2_PARAM_ADDR(ulSaltSourceDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    PBKD2_PARAM_SET(iterations, jLongToCKULong(jIteration));
    PBKD2_PARAM_SET(prf, jLongToCKULong(jPrf));
    jByteArrayToCKByteArray(env, jPrfData,
            (CK_BYTE_PTR *) PBKD2_PARAM_ADDR(pPrfData),
            PBKD2_PARAM_ADDR(ulPrfDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    if (ckParamPtr->version == PARAMS) {
        pUlPasswordLen = calloc(1, sizeof(CK_ULONG));
        if (pUlPasswordLen == NULL) {
            p11ThrowOutOfMemoryError(env, 0);
            goto cleanup;
        }
        ckParamPtr->params.v1.ulPasswordLen = pUlPasswordLen;
    } else {
        pUlPasswordLen = &ckParamPtr->params.v2.ulPasswordLen;
    }
    jCharArrayToCKUTF8CharArray(env, jPassword,
            (CK_CHAR_PTR *) PBKD2_PARAM_ADDR(pPassword),
            pUlPasswordLen);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = (ckParamPtr->version == PARAMS ?
            sizeof(ckParamPtr->params.v1) :
            sizeof(ckParamPtr->params.v2));
    }
    // VersionedPbkd2ParamsPtr is equivalent to CK_PKCS5_PBKD2_PARAMS[2]_PTR
    return ckParamPtr;
cleanup:
    FREE_VERSIONED_PBKD2_MEMBERS(ckParamPtr);
    free(ckParamPtr);
    return NULL;

}

/*
 * converts the Java CK_RSA_PKCS_PSS_PARAMS object to a CK_RSA_PKCS_PSS_PARAMS
 * pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_RSA_PKCS_PSS_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_RSA_PKCS_PSS_PARAMS structure
 */
CK_RSA_PKCS_PSS_PARAMS_PTR
jRsaPkcsPssParamToCKRsaPkcsPssParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_RSA_PKCS_PSS_PARAMS_PTR ckParamPtr;
    jclass jRsaPkcsPssParamsClass;
    jfieldID fieldID;
    jlong jHashAlg, jMgf, jSLen;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jRsaPkcsPssParamsClass = (*env)->FindClass(env, CLASS_RSA_PKCS_PSS_PARAMS);
    if (jRsaPkcsPssParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jRsaPkcsPssParamsClass, "hashAlg", "J");
    if (fieldID == NULL) { return NULL; }
    jHashAlg = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jRsaPkcsPssParamsClass, "mgf", "J");
    if (fieldID == NULL) { return NULL; }
    jMgf = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jRsaPkcsPssParamsClass, "sLen", "J");
    if (fieldID == NULL) { return NULL; }
    jSLen = (*env)->GetLongField(env, jParam, fieldID);

    // allocate memory for CK_RSA_PKCS_PSS_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_RSA_PKCS_PSS_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->hashAlg = jLongToCKULong(jHashAlg);
    ckParamPtr->mgf = jLongToCKULong(jMgf);
    ckParamPtr->sLen = jLongToCKULong(jSLen);
    TRACE1("DEBUG: jRsaPkcsPssParamToCKRsaPkcsPssParam, hashAlg=0x%lX\n", ckParamPtr->hashAlg);
    TRACE1("DEBUG: jRsaPkcsPssParamToCKRsaPkcsPssParam, mgf=0x%lX\n", ckParamPtr->mgf);
    TRACE1("DEBUG: jRsaPkcsPssParamToCKRsaPkcsPssParam, sLen=%lu\n", ckParamPtr->sLen);

    if (pLength != NULL) {
        *pLength = sizeof(CK_RSA_PKCS_PSS_PARAMS);
    }
    return ckParamPtr;

}

/*
 * converts the Java CK_ECDH1_DERIVE_PARAMS object to a CK_ECDH1_DERIVE_PARAMS
 * pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_ECDH1_DERIVE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to nthe new CK_ECDH1_DERIVE_PARAMS structure
 */
CK_ECDH1_DERIVE_PARAMS_PTR
jEcdh1DeriveParamToCKEcdh1DeriveParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_ECDH1_DERIVE_PARAMS_PTR ckParamPtr;
    jclass jEcdh1DeriveParamsClass;
    jfieldID fieldID;
    jlong jLong;
    jobject jSharedData, jPublicData;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jEcdh1DeriveParamsClass = (*env)->FindClass(env, CLASS_ECDH1_DERIVE_PARAMS);
    if (jEcdh1DeriveParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jEcdh1DeriveParamsClass, "kdf", "J");
    if (fieldID == NULL) { return NULL; }
    jLong = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh1DeriveParamsClass, "pSharedData", "[B");
    if (fieldID == NULL) { return NULL; }
    jSharedData = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh1DeriveParamsClass, "pPublicData", "[B");
    if (fieldID == NULL) { return NULL; }
    jPublicData = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_ECDH1_DERIVE_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_ECDH1_DERIVE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->kdf = jLongToCKULong(jLong);
    jByteArrayToCKByteArray(env, jSharedData, &(ckParamPtr->pSharedData),
            &(ckParamPtr->ulSharedDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jPublicData, &(ckParamPtr->pPublicData),
            &(ckParamPtr->ulPublicDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_ECDH1_DERIVE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pSharedData);
    free(ckParamPtr->pPublicData);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_ECDH2_DERIVE_PARAMS object to a CK_ECDH2_DERIVE_PARAMS
 * pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_ECDH2_DERIVE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_ECDH2_DERIVE_PARAMS structure
 */
CK_ECDH2_DERIVE_PARAMS_PTR
jEcdh2DeriveParamToCKEcdh2DeriveParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_ECDH2_DERIVE_PARAMS_PTR ckParamPtr;
    jclass jEcdh2DeriveParamsClass;
    jfieldID fieldID;
    jlong jKdf, jPrivateDataLen, jPrivateData;
    jobject jSharedData, jPublicData, jPublicData2;

    // retrieve java values
    jEcdh2DeriveParamsClass = (*env)->FindClass(env, CLASS_ECDH2_DERIVE_PARAMS);
    if (jEcdh2DeriveParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jEcdh2DeriveParamsClass, "kdf", "J");
    if (fieldID == NULL) { return NULL; }
    jKdf = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh2DeriveParamsClass, "pSharedData", "[B");
    if (fieldID == NULL) { return NULL; }
    jSharedData = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh2DeriveParamsClass, "pPublicData", "[B");
    if (fieldID == NULL) { return NULL; }
    jPublicData = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh2DeriveParamsClass, "ulPrivateDataLen", "J");
    if (fieldID == NULL) { return NULL; }
    jPrivateDataLen = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh2DeriveParamsClass, "hPrivateData", "J");
    if (fieldID == NULL) { return NULL; }
    jPrivateData = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jEcdh2DeriveParamsClass, "pPublicData2", "[B");
    if (fieldID == NULL) { return NULL; }
    jPublicData2 = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_ECDH2_DERIVE_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_ECDH2_DERIVE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->kdf = jLongToCKULong(jKdf);
    jByteArrayToCKByteArray(env, jSharedData, &(ckParamPtr->pSharedData),
            &(ckParamPtr->ulSharedDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jPublicData, &(ckParamPtr->pPublicData),
            &(ckParamPtr->ulPublicDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    ckParamPtr->ulPrivateDataLen = jLongToCKULong(jPrivateDataLen);
    ckParamPtr->hPrivateData = jLongToCKULong(jPrivateData);
    jByteArrayToCKByteArray(env, jPublicData2, &(ckParamPtr->pPublicData2),
            &(ckParamPtr->ulPublicDataLen2));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_ECDH2_DERIVE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pSharedData);
    free(ckParamPtr->pPublicData);
    free(ckParamPtr->pPublicData2);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_X9_42_DH1_DERIVE_PARAMS object to a
 * CK_X9_42_DH1_DERIVE_PARAMS pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_X9_42_DH1_DERIVE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_X9_42_DH1_DERIVE_PARAMS structure
 */
CK_X9_42_DH1_DERIVE_PARAMS_PTR
jX942Dh1DeriveParamToCKX942Dh1DeriveParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_X9_42_DH1_DERIVE_PARAMS_PTR ckParamPtr;
    jclass jX942Dh1DeriveParamsClass;
    jfieldID fieldID;
    jlong jKdf;
    jobject jOtherInfo, jPublicData;

    if (pLength != NULL) {
        *pLength = 0;
    }

    // retrieve java values
    jX942Dh1DeriveParamsClass = (*env)->FindClass(env, CLASS_X9_42_DH1_DERIVE_PARAMS);
    if (jX942Dh1DeriveParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jX942Dh1DeriveParamsClass, "kdf", "J");
    if (fieldID == NULL) { return NULL; }
    jKdf = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh1DeriveParamsClass, "pOtherInfo", "[B");
    if (fieldID == NULL) { return NULL; }
    jOtherInfo = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh1DeriveParamsClass, "pPublicData", "[B");
    if (fieldID == NULL) { return NULL; }
    jPublicData = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_X9_42_DH1_DERIVE_PARAMS pointer
    ckParamPtr = calloc(1, sizeof(CK_X9_42_DH1_DERIVE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->kdf = jLongToCKULong(jKdf);
    jByteArrayToCKByteArray(env, jOtherInfo, &(ckParamPtr->pOtherInfo),
            &(ckParamPtr->ulOtherInfoLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jPublicData, &(ckParamPtr->pPublicData),
            &(ckParamPtr->ulPublicDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_X9_42_DH1_DERIVE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pOtherInfo);
    free(ckParamPtr->pPublicData);
    free(ckParamPtr);
    return NULL;
}

/*
 * converts the Java CK_X9_42_DH2_DERIVE_PARAMS object to a
 * CK_X9_42_DH2_DERIVE_PARAMS pointer
 *
 * @param env - used to call JNI funktions to get the Java classes and objects
 * @param jParam - the Java CK_X9_42_DH2_DERIVE_PARAMS object to convert
 * @param pLength - length of the allocated memory of the returned pointer
 * @return pointer to the new CK_X9_42_DH2_DERIVE_PARAMS structure
 */
CK_X9_42_DH2_DERIVE_PARAMS_PTR
jX942Dh2DeriveParamToCKX942Dh2DeriveParamPtr(JNIEnv *env, jobject jParam, CK_ULONG *pLength)
{
    CK_X9_42_DH2_DERIVE_PARAMS_PTR ckParamPtr;
    jclass jX942Dh2DeriveParamsClass;
    jfieldID fieldID;
    jlong jKdf, jPrivateDataLen, jPrivateData;
    jobject jOtherInfo, jPublicData, jPublicData2;

    if (pLength != NULL) {
        *pLength = 0L;
    }

    // retrieve java values
    jX942Dh2DeriveParamsClass = (*env)->FindClass(env, CLASS_X9_42_DH2_DERIVE_PARAMS);
    if (jX942Dh2DeriveParamsClass == NULL) { return NULL; }
    fieldID = (*env)->GetFieldID(env, jX942Dh2DeriveParamsClass, "kdf", "J");
    if (fieldID == NULL) { return NULL; }
    jKdf = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh2DeriveParamsClass, "pOtherInfo", "[B");
    if (fieldID == NULL) { return NULL; }
    jOtherInfo = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh2DeriveParamsClass, "pPublicData", "[B");
    if (fieldID == NULL) { return NULL; }
    jPublicData = (*env)->GetObjectField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh2DeriveParamsClass, "ulPrivateDataLen", "J");
    if (fieldID == NULL) { return NULL; }
    jPrivateDataLen = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh2DeriveParamsClass, "hPrivateData", "J");
    if (fieldID == NULL) { return NULL; }
    jPrivateData = (*env)->GetLongField(env, jParam, fieldID);
    fieldID = (*env)->GetFieldID(env, jX942Dh2DeriveParamsClass, "pPublicData2", "[B");
    if (fieldID == NULL) { return NULL; }
    jPublicData2 = (*env)->GetObjectField(env, jParam, fieldID);

    // allocate memory for CK_DATE pointer
    ckParamPtr = calloc(1, sizeof(CK_X9_42_DH2_DERIVE_PARAMS));
    if (ckParamPtr == NULL) {
        p11ThrowOutOfMemoryError(env, 0);
        return NULL;
    }

    // populate using java values
    ckParamPtr->kdf = jLongToCKULong(jKdf);
    jByteArrayToCKByteArray(env, jOtherInfo, &(ckParamPtr->pOtherInfo),
            &(ckParamPtr->ulOtherInfoLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    jByteArrayToCKByteArray(env, jPublicData, &(ckParamPtr->pPublicData),
            &(ckParamPtr->ulPublicDataLen));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    ckParamPtr->ulPrivateDataLen = jLongToCKULong(jPrivateDataLen);
    ckParamPtr->hPrivateData = jLongToCKULong(jPrivateData);
    jByteArrayToCKByteArray(env, jPublicData2, &(ckParamPtr->pPublicData2),
            &(ckParamPtr->ulPublicDataLen2));
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    if (pLength != NULL) {
        *pLength = sizeof(CK_X9_42_DH2_DERIVE_PARAMS);
    }
    return ckParamPtr;
cleanup:
    free(ckParamPtr->pOtherInfo);
    free(ckParamPtr->pPublicData);
    free(ckParamPtr->pPublicData2);
    free(ckParamPtr);
    return NULL;
}
