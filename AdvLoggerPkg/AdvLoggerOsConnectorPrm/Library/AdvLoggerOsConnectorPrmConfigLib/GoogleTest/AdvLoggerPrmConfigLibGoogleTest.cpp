/** @file

  This unit tests the AdvLoggerOsConnectorPrmConfigLib

  Copyright (c) Microsoft Corporation
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/DebugLib.h>

  extern PRM_DATA_BUFFER *mStaticDataBuffer;
}

// *----------------------------------------------------------------------------------*
// * Test Contexts                                                                    *
// *----------------------------------------------------------------------------------*

using namespace testing;

/// ================================================================================================
/// ================================================================================================
///
/// TEST CASES
///
/// ================================================================================================
/// ================================================================================================

//
// Declarations for unit tests
//
class AdvLoggerPrmConfigLibTest : public  Test {
  protected:
  EFI_STATUS Status;
  BOOLEAN    Result;
};

/**
  Unit test for MsBootPolicyLibIsSettingsBoot.
**/
TEST_F (AdvLoggerPrmConfigLibTest, AdvLoggerOsConnectorPrmVirtualAddressCallback) {
  PRM_DATA_BUFFER *StaticDataBufferCopy = mStaticDataBuffer;

  mStaticDataBuffer = NULL;
  AdvLoggerOsConnectorPrmVirtualAddressCallback ();
  EXPECT_EQ (mStaticDataBuffer, NULL);
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}