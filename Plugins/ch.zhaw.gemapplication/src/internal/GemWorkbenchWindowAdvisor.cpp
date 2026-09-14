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

#include "GemWorkbenchWindowAdvisor.h"

#include <mitkRenderingManager.h>

#include <QTimer>

void GemWorkbenchWindowAdvisor::PostWindowOpen()
{
  QmitkExtWorkbenchWindowAdvisor::PostWindowOpen();

  // QmitkStdMultiWidgetEditor requests its first update while the top-level
  // window is still being shown. Queue a second request once Qt has entered
  // the event loop so the VTK render windows are interactive immediately.
  QTimer::singleShot(0, [] {
    if (mitk::RenderingManager::IsInstantiated())
      mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  });
}
