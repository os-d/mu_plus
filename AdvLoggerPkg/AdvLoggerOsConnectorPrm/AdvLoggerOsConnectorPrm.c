/** @file AdvLoggerOsConnectorPrm.c

  This driver gives an interface to OS components to fetch/clear the AdvancedLogger memory log.

  Copyright (c) Microsoft Corporation
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PrmModule.h>

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <AdvancedLoggerInternal.h>

typedef struct {
  VOID *OutputBuffer;
  UINT32 OutputBufferSize;
} ADVANCED_LOGGER_PRM_PARAMETER_BUFFER;

//
// PRM Handler GUIDs
//

// {0f8aef11-77b8-4d7f-84cc-fe0cce64ac14}
#define ADVANCED_LOGGER_OS_CONNECTOR_PRM_HANDLER_GUID {0x0f8aef11, 0x77b8, 0x4d7f, {0x84, 0xcc, 0xfe, 0x0c, 0xce, 0x64, 0xac, 0x14}}

/**
    CheckAddress

    The address of the ADVANCE_LOGGER_INFO block pointer is captured before END_OF_DXE.  The
    pointers LogBuffer and LogCurrent, and LogBufferSize, could be written to by untrusted code.  Here, we check that
    the pointers are within the allocated mLoggerInfo space, and that LogBufferSize, which is used in multiple places
    to see if a new message will fit into the log buffer, is valid.

    @param          NONE

    @return         BOOLEAN     TRUE - mInforBlock passes security checks
    @return         BOOLEAN     FALSE- mInforBlock failed security checks

**/
STATIC
BOOLEAN
ValidateInfoBlock (
  ADVANCED_LOGGER_INFO  *LoggerInfo
  )
{
  if (LoggerInfo == NULL) {
    return FALSE;
  }

  if (LoggerInfo->Signature != ADVANCED_LOGGER_SIGNATURE) {
    return FALSE;
  }

  if (LoggerInfo->LogBuffer != (PA_FROM_PTR (LoggerInfo + 1))) {
    return FALSE;
  }

  if ((LoggerInfo->LogCurrent > LoggerInfo->LogBuffer + LoggerInfo->LogBufferSize) ||
      (LoggerInfo->LogCurrent < LoggerInfo->LogBuffer))
  {
    return FALSE;
  }

  return TRUE;
}

/**
  The Advanced Logger Os Connector PRM handler.

  This handler reads the AdvancedLogger buffer and copies the data to the caller supplied buffer.

  @param[in]  ParameterBuffer     A pointer to the PRM handler parameter buffer
  @param[in]  ContextBuffer       A pointer to the PRM handler context buffer

  @retval EFI_STATUS              The PRM handler executed successfully.
  @retval Others                  An error occurred in the PRM handler.

**/
PRM_HANDLER_EXPORT (AdvLoggerOsConnectorPrmHandler) {
  ADVANCED_LOGGER_INFO *LoggerInfo;

  // if (ParameterBuffer == NULL || ContextBuffer == NULL) {
  //   return EFI_INVALID_PARAMETER;
  // }

  if (ContextBuffer->StaticDataBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Verify PRM data buffer signature is valid
  //
  if (
      (ContextBuffer->Signature != PRM_CONTEXT_BUFFER_SIGNATURE) ||
      (ContextBuffer->StaticDataBuffer->Header.Signature != PRM_DATA_BUFFER_HEADER_SIGNATURE))
  {
    return EFI_NOT_FOUND;
  }

  LoggerInfo = *(ADVANCED_LOGGER_INFO **)ContextBuffer->StaticDataBuffer->Data;

  if (!ValidateInfoBlock (LoggerInfo)) {
    return EFI_COMPROMISED_DATA;
  }

  return EFI_SUCCESS;
}

//
// Register the PRM export information for this PRM Module
//
PRM_MODULE_EXPORT (
  PRM_HANDLER_EXPORT_ENTRY (ADVANCED_LOGGER_OS_CONNECTOR_PRM_HANDLER_GUID , AdvLoggerOsConnectorPrmHandler)
  );

/**
  Module entry point.

  @param[in]   ImageHandle     The image handle.
  @param[in]   SystemTable     A pointer to the system table.

  @retval  EFI_SUCCESS         This function always returns success.

**/
EFI_STATUS
EFIAPI
AdvLoggerOsConnectorPrmEntry (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_SUCCESS;
}
