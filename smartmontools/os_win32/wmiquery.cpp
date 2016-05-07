/*
 * os_win32/wmiquery.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2011-13 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#define WINVER 0x0400
#define _WIN32_WINNT WINVER

#include "wmiquery.h"

#include <stdio.h>

const char * wmiquery_cpp_cvsid = "$Id$"
  WMIQUERY_H_CVSID;


/////////////////////////////////////////////////////////////////////////////
// com_bstr

com_bstr::com_bstr(const char * str)
: m_bstr(0)
{
  int sz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, -1, (LPWSTR)0, 0);
  if (sz <= 0)
    return;
  m_bstr = SysAllocStringLen((OLECHAR*)0, sz-1);
  if (!m_bstr)
    return; // throw std::bad_alloc
  MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, -1, m_bstr, sz);
}

bool com_bstr::to_str(const BSTR & bstr, std::string & str)
{
  if (!bstr)
    return false;
  int sz = WideCharToMultiByte(CP_ACP, 0, bstr, -1, (LPSTR)0, 0, (LPCSTR)0, (LPBOOL)0);
  if (sz <= 0)
    return false;
  char * buf = new char[sz];
  WideCharToMultiByte(CP_ACP, 0, bstr, -1, buf, sz, (LPCSTR)0, (LPBOOL)0);
  str = buf;
  delete [] buf;
  return true;
}


/////////////////////////////////////////////////////////////////////////////
// wbem_object

std::string wbem_object::get_str(const char * name) /*const*/
{
  std::string s;
  if (!m_intf)
    return s;

  VARIANT var; VariantInit(&var);
  if (m_intf->Get(com_bstr(name), 0L, &var, (CIMTYPE*)0, (LPLONG)0) /* != WBEM_S_NO_ERROR */)
    return s;

  if (var.vt == VT_BSTR)
    com_bstr::to_str(var.bstrVal, s);
  VariantClear(&var);
  return s;
}


/////////////////////////////////////////////////////////////////////////////
// wbem_enumerator

bool wbem_enumerator::next(wbem_object & obj)
{
  if (!m_intf)
    return false;

  ULONG n = 0;
  HRESULT rc = m_intf->Next(5000 /*5s*/, 1 /*count*/, obj.m_intf.replace(), &n);
  if (FAILED(rc) || n != 1)
    return false;
  return true;
}


/////////////////////////////////////////////////////////////////////////////
// wbem_services

const CLSID xCLSID_WbemLocator = {0x4590f811, 0x1d3a, 0x11d0, {0x89, 0x1f, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24}};
const IID   xIID_IWbemLocator  = {0xdc12a687, 0x737f, 0x11cf, {0x88, 0x4d, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24}};

bool wbem_services::connect()
{
  // Init COM during first call.
  static HRESULT init_rc = -1;
  static bool init_tried = false;
  if (!init_tried) {
    init_tried = true;
    init_rc = CoInitialize((LPVOID)0);
  }
  if (!(init_rc == S_OK  || init_rc == S_FALSE))
    return false;

  /// Create locator.
  com_intf_ptr<IWbemLocator> locator;
  HRESULT rc = CoCreateInstance(xCLSID_WbemLocator, (LPUNKNOWN)0,
    CLSCTX_INPROC_SERVER, xIID_IWbemLocator, (LPVOID*)locator.replace());
  if (FAILED(rc))
    return false;

  // Set timeout flag if supported.
  long flags = 0;
  OSVERSIONINFOA ver; ver.dwOSVersionInfoSize = sizeof(ver);
  if (GetVersionExA(&ver) && ver.dwPlatformId == VER_PLATFORM_WIN32_NT
    && (    ver.dwMajorVersion >= 6 // Vista
        || (ver.dwMajorVersion == 5 && ver.dwMinorVersion >= 1))) // XP
    flags = WBEM_FLAG_CONNECT_USE_MAX_WAIT; // return in 2min or less

  // Connect to local server.
  rc = locator->ConnectServer(com_bstr("\\\\.\\root\\cimv2"),
    (BSTR)0, (BSTR)0, (BSTR)0, // User, Password, Locale
    flags, (BSTR)0, (IWbemContext*)0, m_intf.replace());
  if (FAILED(rc))
    return false;

  // Set authentication information,
  rc = CoSetProxyBlanket(m_intf.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
    (OLECHAR*)0, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
    (RPC_AUTH_IDENTITY_HANDLE*)0, EOAC_NONE);
  if (FAILED(rc)) {
    m_intf.reset();
    return false;
  }

  return true;
}

bool wbem_services::vquery(wbem_enumerator & result, const char * qstr, va_list args) /*const*/
{
  if (!m_intf)
    return false;

  char qline[1024];
  vsnprintf(qline, sizeof(qline), qstr, args);
  qline[sizeof(qline)-1] = 0;

  HRESULT rc = m_intf->ExecQuery(
    com_bstr("WQL"), com_bstr(qline),
    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
    (IWbemContext*)0, result.m_intf.replace());
  if (FAILED(rc))
    return false;

  return true;
}

bool wbem_services::vquery1(wbem_object & obj, const char * qstr, va_list args) /*const*/
{
  wbem_enumerator result;
  if (!vquery(result, qstr, args))
    return false;

  if (!result.next(obj))
    return false;

  wbem_object peek;
  if (result.next(peek))
    return false;

  return true;
}

bool wbem_services::query(wbem_enumerator & result, const char * qstr, ...) /*const*/
{
  va_list args; va_start(args, qstr);
  bool ok = vquery(result, qstr, args);
  va_end(args);
  return ok;
}

bool wbem_services::query1(wbem_object & obj, const char * qstr, ...) /*const*/
{
  va_list args; va_start(args, qstr);
  bool ok = vquery1(obj, qstr, args);
  va_end(args);
  return ok;
}
