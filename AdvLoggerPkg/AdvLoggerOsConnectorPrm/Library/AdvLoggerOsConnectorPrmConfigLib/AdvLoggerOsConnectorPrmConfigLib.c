/** @file

  The boot services environment configuration library for the Adv Logger OS Connector PRM module.

  Copyright (c) Microsoft Corporation
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/PrmConfig.h>
#include <Protocol/AdvancedLogger.h>
#include <Guid/EventGroup.h>
#include <AdvancedLoggerInternal.h>
#include <AdvancedLoggerInternalProtocol.h>

#include <PrmContextBuffer.h>
#include <PrmDataBuffer.h>

// {73807ab1-cab3-40f4-85f7-7ea7146b96d9}
STATIC CONST EFI_GUID  mPrmModuleGuid = {
  0x73807ab1, 0xcab3, 0x40f4, { 0x85, 0xf7, 0x7e, 0xa7, 0x14, 0x6b, 0x96, 0xd9 }
};

// {0f8aef11-77b8-4d7f-84cc-fe0cce64ac14}
STATIC CONST EFI_GUID  mAdvLoggerOsConnectorPrmHandlerGuid = {
  0x0f8aef11, 0x77b8, 0x4d7f, { 0x84, 0xcc, 0xfe, 0x0c, 0xce, 0x64, 0xac, 0x14 }
};

// we need to have a module global of the static data buffer so that we can update the LoggerInfo pointer
// on the virtual address change event
STATIC PRM_DATA_BUFFER           *mStaticDataBuffer = NULL;
STATIC EFI_HANDLE                mPrmConfigProtocolHandle = NULL;
EFI_EVENT   mVirtualAddressChangeEvent;

/**
  Convert internal pointer addresses to virtual addresses.

  @param[in] Event      Event whose notification function is being invoked.
  @param[in] Context    The pointer to the notification function's context, which
                        is implementation-dependent.
**/
STATIC
VOID
EFIAPI
AdvLoggerOsConnectorPrmVirtualAddressCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EfiConvertPointer (0, (VOID **)mStaticDataBuffer->Data);
  DEBUG ((DEBUG_ERROR, "OSDDEBUG converting pointer to: %x\n", *(UINT64 *)mStaticDataBuffer->Data));
}

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
  Constructor of the PRM configuration library.

  @param[in] ImageHandle        The image handle of the driver.
  @param[in] SystemTable        The EFI System Table pointer.

  @retval EFI_SUCCESS           The shell command handlers were installed successfully.
  @retval EFI_UNSUPPORTED       The shell level required was not found.
**/
EFI_STATUS
EFIAPI
AdvLoggerOsConnectorPrmConfigLibConstructor (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  PRM_CONFIG_PROTOCOL       *PrmConfigProtocol = NULL;
  ADVANCED_LOGGER_PROTOCOL  *LoggerProtocol;
  ADVANCED_LOGGER_INFO      **LoggerInfo;
  PRM_CONTEXT_BUFFER        *PrmContextBuffer;
  UINTN                     DataBufferLength;

  //
  // Length of the data buffer = Buffer Header Size + Size of LoggerInfo pointer
  //
  DataBufferLength = sizeof (PRM_DATA_BUFFER_HEADER) + sizeof (ADVANCED_LOGGER_INFO *);

  mStaticDataBuffer = AllocateRuntimeZeroPool (DataBufferLength);
  if (mStaticDataBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize the data buffer header
  //
  mStaticDataBuffer->Header.Signature = PRM_DATA_BUFFER_HEADER_SIGNATURE;
  mStaticDataBuffer->Header.Length    = (UINT32)DataBufferLength;

  //
  // Locate the Logger Information block.
  //
  Status = gBS->LocateProtocol (
                  &gAdvancedLoggerProtocolGuid,
                  NULL,
                  (VOID **)&LoggerProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to find Advanced Logger Protocol\n", __func__));
    goto Done;
  }

  LoggerInfo  = (ADVANCED_LOGGER_INFO **)mStaticDataBuffer->Data;
  *LoggerInfo = LOGGER_INFO_FROM_PROTOCOL (LoggerProtocol);

  if (!ValidateInfoBlock (*LoggerInfo)) {
    DEBUG ((DEBUG_ERROR, "AdvLoggerOsConnectorPrmConfigLib Failed to validate AdvLogger region\n"));
    *LoggerInfo = 0;
    goto Done;
  }

  DEBUG ((DEBUG_ERROR, "OSDDEBUG got Advanced Logger Info buffer: %x\n", *(UINT64 *)(mStaticDataBuffer->Data)));

  //
  // Allocate and populate the context buffer
  //

  //
  // This context buffer is not actually used by PRM handler at OS runtime. The OS will allocate
  // the actual context buffer passed to the PRM handler.
  //
  // This context buffer is used internally in the firmware to associate a PRM handler with a
  // a static data buffer and a runtime MMIO ranges array so those can be placed into the
  // PRM_HANDLER_INFORMATION_STRUCT and PRM_MODULE_INFORMATION_STRUCT respectively for the PRM handler.
  //
  PrmContextBuffer = AllocateZeroPool (sizeof (*PrmContextBuffer));
  ASSERT (PrmContextBuffer != NULL);
  if (PrmContextBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  CopyGuid (&PrmContextBuffer->HandlerGuid, &mAdvLoggerOsConnectorPrmHandlerGuid);
  PrmContextBuffer->Signature = PRM_CONTEXT_BUFFER_SIGNATURE;
  PrmContextBuffer->Version   = PRM_CONTEXT_BUFFER_INTERFACE_VERSION;

  PrmConfigProtocol = AllocateZeroPool (sizeof (*PrmConfigProtocol));
  ASSERT (PrmConfigProtocol != NULL);
  if (PrmConfigProtocol == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  CopyGuid (&PrmConfigProtocol->ModuleContextBuffers.ModuleGuid, &mPrmModuleGuid);
  PrmConfigProtocol->ModuleContextBuffers.BufferCount = 1;
  PrmConfigProtocol->ModuleContextBuffers.Buffer      = PrmContextBuffer;
  PrmContextBuffer->StaticDataBuffer = mStaticDataBuffer;

  //
  // Install the PRM Configuration Protocol for this module. This indicates the configuration
  // library has completed resource initialization for the PRM module.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mPrmConfigProtocolHandle,
                  &gPrmConfigProtocolGuid,
                  (VOID *)PrmConfigProtocol,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a failed to install config protocol\n", __func__));
    goto Done;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  AdvLoggerOsConnectorPrmVirtualAddressCallback,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a failed to register for virtual address callback Status %r\n", Status));
  }

Done:
  if (EFI_ERROR (Status)) {
    if (PrmContextBuffer != NULL) {
      FreePool (PrmContextBuffer);
    }

    if (PrmConfigProtocol != NULL) {
      FreePool (PrmConfigProtocol);
    }
  }

  // if we failed to setup the PRM, we should still boot as this is a diagnostic fetching mechanism
  // we've logged (the log we won't be able to fetch) and that's all we can do
  // however, we've freed the context buffer, so we won't be passing an invalid LoggerInfo pointer to the PRM module
  return EFI_SUCCESS;
}
