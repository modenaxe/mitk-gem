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
#include <berryIPerspectiveDescriptor.h>
#include <berryIWorkbenchPage.h>
#include <berryIWorkbenchWindow.h>
#include <berryIWorkbenchWindowConfigurer.h>
#include <mitkCoreServices.h>
#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkIDataStorageReference.h>
#include <mitkIDataStorageService.h>
#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <mitkRenderingManager.h>

#include <QTimer>

namespace
{
  constexpr auto GemPerspectiveId = "org.mitk.perspectives.gem";
  constexpr auto FemExportViewId = "org.mitk.views.femexport";
  constexpr auto LayoutPreferencesNode = "ch.zhaw.gemapplication/layout";
  constexpr auto FemExportLayoutMigrationKey = "fem export tab v1";

  void EnsureFemExportViewIsInDefaultLayout(const berry::IWorkbenchWindow::Pointer& window)
  {
    if (window.IsNull())
      return;

    const auto page = window->GetActivePage();
    if (page.IsNull())
      return;

    const auto perspective = page->GetPerspective();
    if (perspective.IsNull() || perspective->GetId() != GemPerspectiveId)
      return;

    auto* preferencesService = mitk::CoreServices::GetPreferencesService();
    if (preferencesService == nullptr)
      return;

    auto* preferences =
      preferencesService->GetSystemPreferences()->Node(LayoutPreferencesNode);
    if (preferences->GetBool(FemExportLayoutMigrationKey, false))
      return;

    // BlueBerry restores the user's saved perspective instead of invoking
    // GemPerspective::CreateInitialLayout. Profiles created before FEM Export
    // was added therefore never receive its tab. Reset only those old layouts;
    // the current factory places FEM Export immediately after Material Mapping.
    if (page->FindViewReference(FemExportViewId).IsNull())
      page->ResetPerspective();

    // Persist the migration only after the requested view is present. This
    // keeps the operation one-shot while still allowing users to customize or
    // close the tab later without having their layout reset on every launch.
    if (page->FindViewReference(FemExportViewId).IsNotNull())
    {
      preferences->PutBool(FemExportLayoutMigrationKey, true);
      preferences->Flush();
    }
  }

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

  EnsureFemExportViewIsInDefaultLayout(GetWindowConfigurer()->GetWindow());

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
