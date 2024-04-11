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
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/PrmConfig.h>
#include <Protocol/AdvancedLogger.h>
#include <Guid/EventGroup.h>
#include <PiDxe.h>
#include <AdvancedLoggerInternal.h>
#include <AdvancedLoggerInternalProtocol.h>

#include <PrmContextBuffer.h>
#include <PrmDataBuffer.h>

// {B4DFA4A2-EAD0-4F55-998B-EA5BE68F73FD}
STATIC CONST EFI_GUID  mPrmModuleGuid = {
  0xb4dfa4a2, 0xead0, 0x4f55, { 0x99, 0x8b, 0xea, 0x5b, 0xe6, 0x8f, 0x73, 0xfd }
};

// {0f8aef11-77b8-4d7f-84cc-fe0cce64ac14}
STATIC CONST EFI_GUID  mAdvLoggerOsConnectorPrmHandlerGuid = {
  0x0f8aef11, 0x77b8, 0x4d7f, { 0x84, 0xcc, 0xfe, 0x0c, 0xce, 0x64, 0xac, 0x14 }
};

// we need to have a module global of the static data buffer so that we can update the LoggerInfo pointer
// on the virtual address change event
STATIC PRM_DATA_BUFFER           *mStaticDataBuffer = NULL;
STATIC EFI_HANDLE                mPrmConfigProtocolHandle = NULL;
// EFI_EVENT   mVirtualAddressChangeEvent;

// volatile BOOLEAN DbgLoop = 1;

/**
  Convert internal pointer addresses to virtual addresses.

  @param[in] Event      Event whose notification function is being invoked.
  @param[in] Context    The pointer to the notification function's context, which
                        is implementation-dependent.
**/
// VOID
// EFIAPI
// AdvLoggerOsConnectorPrmVirtualAddressCallback (
//   IN  EFI_EVENT  Event,
//   IN  VOID       *Context
//   )
// {
//   while (DbgLoop) {}
//   EfiConvertPointer (0, (VOID **)mStaticDataBuffer->Data);
//   DEBUG ((DEBUG_ERROR, "OSDDEBUG converting pointer to: %x\n", *(UINT64 *)mStaticDataBuffer->Data));
// }

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
  ADVANCED_LOGGER_INFO      *LoggerInfo;
  PRM_CONTEXT_BUFFER        *PrmContextBuffer;
  UINTN                     DataBufferLength;
  // EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor; 

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

  mStaticDataBuffer = (PRM_DATA_BUFFER *)LOGGER_INFO_FROM_PROTOCOL (LoggerProtocol);
  LoggerInfo = (ADVANCED_LOGGER_INFO *)mStaticDataBuffer;
  if (!ValidateInfoBlock (LoggerInfo)) {
    DEBUG ((DEBUG_ERROR, "AdvLoggerOsConnectorPrmConfigLib Failed to validate AdvLogger region\n"));
    mStaticDataBuffer = 0;
    goto Done;
  }
  mStaticDataBuffer = (PRM_DATA_BUFFER *)((CHAR8 *)mStaticDataBuffer - 8);

  DEBUG ((DEBUG_ERROR, "OSDDEBUG 70 got mStaticDataBuffer: %llx\n", mStaticDataBuffer));

  //
  // Length of the data buffer = Buffer Header Size + Size of LoggerInfo pointer
  //
  DataBufferLength = sizeof (PRM_DATA_BUFFER_HEADER) + LoggerInfo->LogBufferSize + sizeof (*LoggerInfo);

  //
  // Initialize the data buffer header
  //
  mStaticDataBuffer->Header.Signature = PRM_DATA_BUFFER_HEADER_SIGNATURE;
  mStaticDataBuffer->Header.Length    = (UINT32)DataBufferLength;

  DEBUG ((DEBUG_ERROR, "OSDDEBUG 70 got mStaticDataBuffer: %llx Length: %x\n", mStaticDataBuffer, mStaticDataBuffer->Header.Length));

  // Status = gDS->GetMemorySpaceDescriptor ((EFI_PHYSICAL_ADDRESS)*LoggerInfo, &Descriptor);
  // if (EFI_ERROR (Status)) {
  //   DEBUG ((
  //     DEBUG_ERROR,
  //     "%a: Error [%r] finding descriptor for advanced logger info 0x%016x.\n",
  //     __func__,
  //     Status,
  //     *LoggerInfo
  //     ));
  //   goto Done;
  // }

  // DEBUG ((DEBUG_ERROR, "OSDDEBUG got Advanced Logger Info buffer: %x Descriptor Attributes: %llx Descriptor Type: %llx\n", *(UINT64 *)(mStaticDataBuffer->Data), Descriptor.Attributes));

  // if ((Descriptor.Attributes & EFI_MEMORY_RUNTIME) == 0) {
  //   Status = gDS->SetMemorySpaceAttributes (
  //                   ((EFI_PHYSICAL_ADDRESS)*LoggerInfo) & ~(EFI_PAGE_MASK),
  //                   ALIGN_VALUE ((*LoggerInfo)->LogBufferSize, EFI_PAGE_SIZE),
  //                   // MU_CHANGE START: The memory space descriptor access attributes are not accurate. Don't pass
  //                   //                  in access attributes so SetMemorySpaceAttributes() doesn't update them.
  //                   //                  EFI_MEMORY_RUNTIME is not a CPU arch attribute, so calling
  //                   //                  SetMemorySpaceAttributes() with only it set will not clear existing page table
  //                   //                  attributes for this region, such as EFI_MEMORY_XP
  //                   // Descriptor.Attributes | EFI_MEMORY_RUNTIME
  //                   EFI_MEMORY_RUNTIME
  //                   // MU_CHANGE END
  //                   );
  //   if (EFI_ERROR (Status)) {
  //     DEBUG ((
  //       DEBUG_ERROR,
  //       "%a: Error [%r] setting EFI_MEMORY_RUNTIME for LoggerInfo 0x%016x.\n",
  //       __func__,
  //       Status,
  //       *LoggerInfo
  //       ));
  //     goto Done;
  //   }
  // }

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

  // Status = gBS->CreateEventEx (
  //                 EVT_NOTIFY_SIGNAL,
  //                 TPL_NOTIFY,
  //                 AdvLoggerOsConnectorPrmVirtualAddressCallback,
  //                 NULL,
  //                 &gEfiEventVirtualAddressChangeGuid,
  //                 &mVirtualAddressChangeEvent
  //                 );

  // if (EFI_ERROR (Status)) {
  //   DEBUG ((DEBUG_ERROR, "%a failed to register for virtual address callback Status %r\n", Status));
  // }

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
