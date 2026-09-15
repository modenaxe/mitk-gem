/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#pragma once

#include <QmitkExtWorkbenchWindowAdvisor.h>

class GemInitialDataReinitObserver;

/**
 * Application-specific workbench startup hooks.
 *
 * MITK opens the multi-widget editor while the workbench is still becoming
 * visible. A queued update makes the first interactive frame deterministic on
 * current Qt/VTK combinations instead of waiting for a later UI event.
 */
class GemWorkbenchWindowAdvisor final : public QmitkExtWorkbenchWindowAdvisor
{
public:
  using QmitkExtWorkbenchWindowAdvisor::QmitkExtWorkbenchWindowAdvisor;

  ~GemWorkbenchWindowAdvisor() override;

  void PostWindowOpen() override;

private:
  // Defined and destroyed in the implementation file, after the observer's
  // complete type is available. This advisor's inherited constructor is
  // inline, so std::unique_ptr would otherwise require that type in every
  // translation unit including this header.
  GemInitialDataReinitObserver* m_InitialDataReinitObserver = nullptr;
};
