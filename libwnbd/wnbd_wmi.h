/*
 * Copyright (c) 2019 SUSE LLC
 *
 * Licensed under LGPL-2.1 (see LICENSE)
 */

#pragma once
#include <comutil.h>

#define _WIN32_DCOM
#include <wbemcli.h>

#include <string>
#include <vector>

typedef struct
{
    IWbemLocator* WbemLoc;
    IWbemServices* WbemSvc;
} WMI_CONNECTION, *PWMI_CONNECTION;

// We'll fetch other fields if/when needed.
// This is only used internally.
typedef struct
{
  std::wstring deviceId;
  uint32_t Index;
} DiskInfo;

HRESULT CoInitializeBasic();
VOID CloseWmiConnecton(PWMI_CONNECTION Connection);
HRESULT CreateWmiConnection(
    LPCWSTR Namespace,
    PWMI_CONNECTION Connection);

HRESULT GetPropertyStr(
    IWbemClassObject* ClsObj,
    const std::wstring property,
    std::wstring& value);
HRESULT GetPropertyInt(
    IWbemClassObject* ClsObj,
    const std::wstring& property,
    UINT32& value);

HRESULT GetDiskDrives(
    PWMI_CONNECTION Connection,
    std::wstring Query,
    std::vector<DiskInfo>& Disks);
HRESULT GetDiskNumberBySerialNumber(
    LPCWSTR SerialNumber,
    PDWORD DiskNumber);
