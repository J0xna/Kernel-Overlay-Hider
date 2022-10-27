#pragma once
#include <ntifs.h>
#include <ntddk.h>
#include <ntdef.h>
#include <windef.h>
#include <ntimage.h>
#include <cstdint>
#include <cstddef>
#include <intrin.h>
#include <wingdi.h>
#include <ntstrsafe.h>

#include "skcrypt.h"

#pragma comment(lib, "ntoskrnl.lib")

#ifdef _DEBUG
#define log( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[DEBUG-LOG] " s), __VA_ARGS__ )
#define log_s( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[+] " s), __VA_ARGS__ )
#define log_e( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[-] " s), __VA_ARGS__ )
#define log_w( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[!] " s), __VA_ARGS__ )
#else
#define log( s, ... ) ( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[DEBUG-LOG] " s), __VA_ARGS__ )
#define log_s( s, ... ) ( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[+] " s), __VA_ARGS__ )
#define log_e( s, ... ) ( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[-] " s), __VA_ARGS__ )
#define log_w( s, ... ) ( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, _("[!] " s), __VA_ARGS__ )
#endif