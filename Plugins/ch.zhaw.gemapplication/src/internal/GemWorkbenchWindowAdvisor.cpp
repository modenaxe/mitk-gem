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
#include "QmitkExtApplicationPlugin.h"

#include <ctkPluginContext.h>
#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkIDataStorageReference.h>
#include <mitkIDataStorageService.h>
#include <mitkRenderingManager.h>

#include <QTimer>

namespace
{
  bool IsDisplayableDataNode(const mitk::DataNode* node)
  {
    if (node == nullptr || node->GetData() == nullptr)
    {
      return false;
    }

    bool isHelperObject = false;
    node->GetBoolProperty("helper object", isHelperObject);
    return !isHelperObject;
  }
}

class GemInitialDataReinitObserver final : public QObject
{
public:
  explicit GemInitialDataReinitObserver(QObject* parent)
    : QObject(parent)
  {
  }

  ~GemInitialDataReinitObserver() override
  {
    if (m_DataStorage.IsNotNull())
    {
      m_DataStorage->AddNodeEvent.RemoveListener(
        mitk::MessageDelegate1<GemInitialDataReinitObserver, const mitk::DataNode*>(
          this, &GemInitialDataReinitObserver::OnNodeAdded));
    }
  }

  void Observe(mitk::DataStorage::Pointer dataStorage)
  {
    if (m_DataStorage == dataStorage)
    {
      return;
    }

    if (m_DataStorage.IsNotNull())
    {
      m_DataStorage->AddNodeEvent.RemoveListener(
        mitk::MessageDelegate1<GemInitialDataReinitObserver, const mitk::DataNode*>(
          this, &GemInitialDataReinitObserver::OnNodeAdded));
    }

    m_DataStorage = dataStorage;
    m_InitialReinitDone = false;
    m_ReinitQueued = false;

    if (m_DataStorage.IsNotNull())
    {
      m_DataStorage->AddNodeEvent.AddListener(
        mitk::MessageDelegate1<GemInitialDataReinitObserver, const mitk::DataNode*>(
          this, &GemInitialDataReinitObserver::OnNodeAdded));

      const auto nodes = m_DataStorage->GetAll();
      for (auto it = nodes->Begin(); it != nodes->End(); ++it)
      {
        if (IsDisplayableDataNode(it->Value()))
        {
          QueueInitialReinit();
          break;
        }
      }
    }
  }

private:
  void OnNodeAdded(const mitk::DataNode* node)
  {
    if (!m_InitialReinitDone && IsDisplayableDataNode(node))
    {
      QueueInitialReinit();
    }
  }

  void QueueInitialReinit()
  {
    if (m_ReinitQueued || m_InitialReinitDone)
    {
      return;
    }

    m_ReinitQueued = true;
    QTimer::singleShot(0, this, [this] {
      m_ReinitQueued = false;
      if (m_DataStorage.IsNull() || !mitk::RenderingManager::IsInstantiated())
      {
        return;
      }

      // Data Manager's manual "Reinit" command follows this same code path.
      // Doing it once, after the first real data node is added, gives the
      // newly opened standard multi-widget correct geometry and camera bounds.
      mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(m_DataStorage);
      mitk::RenderingManager::GetInstance()->RequestUpdateAll();
      m_InitialReinitDone = true;
    });
  }

  mitk::DataStorage::Pointer m_DataStorage;
  bool m_InitialReinitDone = false;
  bool m_ReinitQueued = false;
};

GemWorkbenchWindowAdvisor::~GemWorkbenchWindowAdvisor()
{
  delete m_InitialDataReinitObserver;
}

void GemWorkbenchWindowAdvisor::PostWindowOpen()
{
  QmitkExtWorkbenchWindowAdvisor::PostWindowOpen();

  // File loading happens after the multi-widget is created. MITK's usual
  // startup reinit therefore sees only helper geometry and leaves the first
  // imported image without useful camera bounds. Observe the active storage
  // and perform that reinit once when the first real data node arrives.
  auto* applicationPlugin = QmitkExtApplicationPlugin::GetDefault();
  auto* context = applicationPlugin == nullptr ? nullptr : applicationPlugin->GetPluginContext();
  if (context != nullptr)
  {
    const auto serviceReference = context->getServiceReference<mitk::IDataStorageService>();
    auto* dataStorageService = serviceReference
      ? context->getService<mitk::IDataStorageService>(serviceReference)
      : nullptr;
    const auto dataStorageReference = dataStorageService == nullptr
      ? mitk::IDataStorageReference::Pointer()
      : dataStorageService->GetDataStorage();

    if (dataStorageReference.IsNotNull())
    {
      delete m_InitialDataReinitObserver;
      m_InitialDataReinitObserver = new GemInitialDataReinitObserver(this);
      m_InitialDataReinitObserver->Observe(dataStorageReference->GetDataStorage());
    }
  }

  // QmitkStdMultiWidgetEditor requests its first update while the top-level
  // window is still being shown. Queue a second request once Qt has entered
  // the event loop so the VTK render windows are interactive immediately.
  QTimer::singleShot(0, [] {
    if (mitk::RenderingManager::IsInstantiated())
      mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  });
}
