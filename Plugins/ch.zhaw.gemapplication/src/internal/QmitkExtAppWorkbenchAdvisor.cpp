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

#include "QmitkExtAppWorkbenchAdvisor.h"
#include "GemWorkbenchWindowAdvisor.h"
#include "internal/QmitkExtApplicationPlugin.h"

#include <QmitkExtWorkbenchWindowAdvisor.h>
#include <berryQtPreferences.h>
#include <berryWorkbenchPlugin.h>
#include <mitkIPreferences.h>
#include "WorkbenchUtils.h"

const QString QmitkExtAppWorkbenchAdvisor::DEFAULT_PERSPECTIVE_ID = "org.mitk.perspectives.gem";

void
QmitkExtAppWorkbenchAdvisor::Initialize(berry::IWorkbenchConfigurer::Pointer configurer)
{
  // MITK defaults to its dark style. GEM's shipped icons and controls are
  // authored for a light canvas, so use the Light style for a new profile.
  // An explicit style selected by the user remains untouched.
  auto* stylePreferences = berry::WorkbenchPlugin::GetDefault()->GetPreferences()->Node(
    berry::QtPreferences::QT_STYLES_NODE);
  if (stylePreferences->Get(berry::QtPreferences::QT_STYLE_NAME, "").empty())
  {
    stylePreferences->Put(berry::QtPreferences::QT_STYLE_NAME,
                          ":/org.blueberry.ui.qt/lightstyle.qss");
    stylePreferences->Flush();
  }

  berry::QtWorkbenchAdvisor::Initialize(configurer);
  configurer->SetSaveAndRestore(true);
}

berry::WorkbenchWindowAdvisor*
QmitkExtAppWorkbenchAdvisor::CreateWorkbenchWindowAdvisor(berry::IWorkbenchWindowConfigurer::Pointer configurer)
{
  auto* advisor = new GemWorkbenchWindowAdvisor(this, configurer);

  // Exclude the help perspective from org.blueberry.ui.qt.help from
  // the normal perspective list.
  // The perspective gets a dedicated menu entry in the help menu
  QList<QString> excludePerspectives;
  excludePerspectives.push_back("org.blueberry.perspectives.help");
  excludePerspectives.push_back("org.mitk.mitkworkbench.perspectives.editor");
  excludePerspectives.push_back("org.mitk.mitkworkbench.perspectives.visualization");
  advisor->SetPerspectiveExcludeList(excludePerspectives);

  // Exclude some views from the normal view list
  QList<QString> excludeViews;
  excludeViews.push_back("org.mitk.views.modules");
  excludeViews.push_back("org.blueberry.views.helpindex");
  excludeViews.push_back("org.blueberry.views.helpsearch");
  excludeViews.push_back("org.mitk.views.deformableclippingplane");
  excludeViews.push_back("org.mitk.views.datamanager");
  advisor->SetViewExcludeList(excludeViews);

  // general settings
  advisor->SetWindowIcon(":/ch.zhaw.gemapplication/icon.png");
  auto titleString = "MITK-GEM " + WorkbenchUtils::getGemVersion();
  advisor->SetProductName(titleString.c_str());
  advisor->ShowVersionInfo(false);
  advisor->ShowMitkVersionInfo(false);
  return advisor;
}

QString QmitkExtAppWorkbenchAdvisor::GetInitialWindowPerspectiveId()
{
  return DEFAULT_PERSPECTIVE_ID;
}
