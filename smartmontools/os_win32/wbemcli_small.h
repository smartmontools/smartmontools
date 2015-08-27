/*
 * os_win32/wbemcli_small.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * This file was extracted from wbemcli.h of the w64 mingw-runtime package
 * (http://mingw-w64.sourceforge.net/). See original copyright below.
 */

/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error This stub requires an updated version of <rpcndr.h>
#endif

#include "windows.h"
#include "ole2.h"

#ifndef __wbemcli_h__
#define __wbemcli_h__

#if !defined(__cplusplus) || defined(CINTERFACE)
#error C++ interfaces only
#endif

typedef struct IWbemQualifierSet IWbemQualifierSet;
typedef struct IWbemObjectSink IWbemObjectSink;
typedef struct IEnumWbemClassObject IEnumWbemClassObject;
typedef struct IWbemCallResult IWbemCallResult;
typedef struct IWbemContext IWbemContext;

extern "C" {

  typedef enum tag_WBEM_GENERIC_FLAG_TYPE {
    WBEM_FLAG_RETURN_IMMEDIATELY = 0x10,WBEM_FLAG_RETURN_WBEM_COMPLETE = 0,WBEM_FLAG_BIDIRECTIONAL = 0,WBEM_FLAG_FORWARD_ONLY = 0x20,
    WBEM_FLAG_NO_ERROR_OBJECT = 0x40,WBEM_FLAG_RETURN_ERROR_OBJECT = 0,WBEM_FLAG_SEND_STATUS = 0x80,WBEM_FLAG_DONT_SEND_STATUS = 0,
    WBEM_FLAG_ENSURE_LOCATABLE = 0x100,WBEM_FLAG_DIRECT_READ = 0x200,WBEM_FLAG_SEND_ONLY_SELECTED = 0,WBEM_RETURN_WHEN_COMPLETE = 0,
    WBEM_RETURN_IMMEDIATELY = 0x10,WBEM_MASK_RESERVED_FLAGS = 0x1f000,WBEM_FLAG_USE_AMENDED_QUALIFIERS = 0x20000,
    WBEM_FLAG_STRONG_VALIDATION = 0x100000
  } WBEM_GENERIC_FLAG_TYPE;

  typedef long CIMTYPE;

  struct IWbemClassObject : public IUnknown {
  public:
    virtual HRESULT WINAPI GetQualifierSet(IWbemQualifierSet **ppQualSet) = 0;
    virtual HRESULT WINAPI Get(LPCWSTR wszName,long lFlags,VARIANT *pVal,CIMTYPE *pType,long *plFlavor) = 0;
    virtual HRESULT WINAPI Put(LPCWSTR wszName,long lFlags,VARIANT *pVal,CIMTYPE Type) = 0;
    virtual HRESULT WINAPI Delete(LPCWSTR wszName) = 0;
    virtual HRESULT WINAPI GetNames(LPCWSTR wszQualifierName,long lFlags,VARIANT *pQualifierVal,SAFEARRAY **pNames) = 0;
    virtual HRESULT WINAPI BeginEnumeration(long lEnumFlags) = 0;
    virtual HRESULT WINAPI Next(long lFlags,BSTR *strName,VARIANT *pVal,CIMTYPE *pType,long *plFlavor) = 0;
    virtual HRESULT WINAPI EndEnumeration(void) = 0;
    virtual HRESULT WINAPI GetPropertyQualifierSet(LPCWSTR wszProperty,IWbemQualifierSet **ppQualSet) = 0;
    virtual HRESULT WINAPI Clone(IWbemClassObject **ppCopy) = 0;
    virtual HRESULT WINAPI GetObjectText(long lFlags,BSTR *pstrObjectText) = 0;
    virtual HRESULT WINAPI SpawnDerivedClass(long lFlags,IWbemClassObject **ppNewClass) = 0;
    virtual HRESULT WINAPI SpawnInstance(long lFlags,IWbemClassObject **ppNewInstance) = 0;
    virtual HRESULT WINAPI CompareTo(long lFlags,IWbemClassObject *pCompareTo) = 0;
    virtual HRESULT WINAPI GetPropertyOrigin(LPCWSTR wszName,BSTR *pstrClassName) = 0;
    virtual HRESULT WINAPI InheritsFrom(LPCWSTR strAncestor) = 0;
    virtual HRESULT WINAPI GetMethod(LPCWSTR wszName,long lFlags,IWbemClassObject **ppInSignature,IWbemClassObject **ppOutSignature) = 0;
    virtual HRESULT WINAPI PutMethod(LPCWSTR wszName,long lFlags,IWbemClassObject *pInSignature,IWbemClassObject *pOutSignature) = 0;
    virtual HRESULT WINAPI DeleteMethod(LPCWSTR wszName) = 0;
    virtual HRESULT WINAPI BeginMethodEnumeration(long lEnumFlags) = 0;
    virtual HRESULT WINAPI NextMethod(long lFlags,BSTR *pstrName,IWbemClassObject **ppInSignature,IWbemClassObject **ppOutSignature) = 0;
    virtual HRESULT WINAPI EndMethodEnumeration(void) = 0;
    virtual HRESULT WINAPI GetMethodQualifierSet(LPCWSTR wszMethod,IWbemQualifierSet **ppQualSet) = 0;
    virtual HRESULT WINAPI GetMethodOrigin(LPCWSTR wszMethodName,BSTR *pstrClassName) = 0;
  };

  struct IWbemServices : public IUnknown {
  public:
    virtual HRESULT WINAPI OpenNamespace(const BSTR strNamespace,long lFlags,IWbemContext *pCtx,IWbemServices **ppWorkingNamespace,IWbemCallResult **ppResult) = 0;
    virtual HRESULT WINAPI CancelAsyncCall(IWbemObjectSink *pSink) = 0;
    virtual HRESULT WINAPI QueryObjectSink(long lFlags,IWbemObjectSink **ppResponseHandler) = 0;
    virtual HRESULT WINAPI GetObject(const BSTR strObjectPath,long lFlags,IWbemContext *pCtx,IWbemClassObject **ppObject,IWbemCallResult **ppCallResult) = 0;
    virtual HRESULT WINAPI GetObjectAsync(const BSTR strObjectPath,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI PutClass(IWbemClassObject *pObject,long lFlags,IWbemContext *pCtx,IWbemCallResult **ppCallResult) = 0;
    virtual HRESULT WINAPI PutClassAsync(IWbemClassObject *pObject,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI DeleteClass(const BSTR strClass,long lFlags,IWbemContext *pCtx,IWbemCallResult **ppCallResult) = 0;
    virtual HRESULT WINAPI DeleteClassAsync(const BSTR strClass,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI CreateClassEnum(const BSTR strSuperclass,long lFlags,IWbemContext *pCtx,IEnumWbemClassObject **ppEnum) = 0;
    virtual HRESULT WINAPI CreateClassEnumAsync(const BSTR strSuperclass,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI PutInstance(IWbemClassObject *pInst,long lFlags,IWbemContext *pCtx,IWbemCallResult **ppCallResult) = 0;
    virtual HRESULT WINAPI PutInstanceAsync(IWbemClassObject *pInst,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI DeleteInstance(const BSTR strObjectPath,long lFlags,IWbemContext *pCtx,IWbemCallResult **ppCallResult) = 0;
    virtual HRESULT WINAPI DeleteInstanceAsync(const BSTR strObjectPath,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI CreateInstanceEnum(const BSTR strFilter,long lFlags,IWbemContext *pCtx,IEnumWbemClassObject **ppEnum) = 0;
    virtual HRESULT WINAPI CreateInstanceEnumAsync(const BSTR strFilter,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI ExecQuery(const BSTR strQueryLanguage,const BSTR strQuery,long lFlags,IWbemContext *pCtx,IEnumWbemClassObject **ppEnum) = 0;
    virtual HRESULT WINAPI ExecQueryAsync(const BSTR strQueryLanguage,const BSTR strQuery,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI ExecNotificationQuery(const BSTR strQueryLanguage,const BSTR strQuery,long lFlags,IWbemContext *pCtx,IEnumWbemClassObject **ppEnum) = 0;
    virtual HRESULT WINAPI ExecNotificationQueryAsync(const BSTR strQueryLanguage,const BSTR strQuery,long lFlags,IWbemContext *pCtx,IWbemObjectSink *pResponseHandler) = 0;
    virtual HRESULT WINAPI ExecMethod(const BSTR strObjectPath,const BSTR strMethodName,long lFlags,IWbemContext *pCtx,IWbemClassObject *pInParams,IWbemClassObject **ppOutParams,IWbemCallResult **ppCallResult) = 0;
    virtual HRESULT WINAPI ExecMethodAsync(const BSTR strObjectPath,const BSTR strMethodName,long lFlags,IWbemContext *pCtx,IWbemClassObject *pInParams,IWbemObjectSink *pResponseHandler) = 0;
  };

  EXTERN_C const IID IID_IWbemLocator;
  struct IWbemLocator : public IUnknown {
  public:
    virtual HRESULT WINAPI ConnectServer(const BSTR strNetworkResource,const BSTR strUser,const BSTR strPassword,const BSTR strLocale,long lSecurityFlags,const BSTR strAuthority,IWbemContext *pCtx,IWbemServices **ppNamespace) = 0;
  };

  struct IEnumWbemClassObject : public IUnknown {
  public:
    virtual HRESULT WINAPI Reset(void) = 0;
    virtual HRESULT WINAPI Next(long lTimeout,ULONG uCount,IWbemClassObject **apObjects,ULONG *puReturned) = 0;
    virtual HRESULT WINAPI NextAsync(ULONG uCount,IWbemObjectSink *pSink) = 0;
    virtual HRESULT WINAPI Clone(IEnumWbemClassObject **ppEnum) = 0;
    virtual HRESULT WINAPI Skip(long lTimeout,ULONG nCount) = 0;
  };

  EXTERN_C const CLSID CLSID_WbemLocator;

  typedef enum tag_WBEM_CONNECT_OPTIONS {
    WBEM_FLAG_CONNECT_REPOSITORY_ONLY = 0x40,WBEM_FLAG_CONNECT_USE_MAX_WAIT = 0x80,WBEM_FLAG_CONNECT_PROVIDERS = 0x100
  } WBEM_CONNECT_OPTIONS;

}
#endif
