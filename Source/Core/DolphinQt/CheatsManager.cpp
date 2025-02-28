// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/CheatsManager.h"

#include <functional>

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "Core/ActionReplay.h"
#include "Core/CheatSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "UICommon/GameFile.h"

#include "DolphinQt/CheatSearchFactoryWidget.h"
#include "DolphinQt/CheatSearchWidget.h"
#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/Config/PrimeCheatsWidget.h"
#include "DolphinQt/Settings.h"
#include "QtUtils/PartiallyClosableTabWidget.h"

CheatsManager::CheatsManager(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Cheats Manager"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &CheatsManager::OnStateChanged);

  CreateWidgets();
  ConnectWidgets();

  RefreshCodeTabs(Core::GetState(), true);

  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("cheatsmanager/geometry")).toByteArray());
}

CheatsManager::~CheatsManager()
{
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("cheatsmanager/geometry"), saveGeometry());
}

void CheatsManager::OnStateChanged(Core::State state)
{
  RefreshCodeTabs(state, false);
}

void CheatsManager::RefreshCodeTabs(Core::State state, bool force)
{
  if (!force && (state == Core::State::Starting || state == Core::State::Stopping))
    return;

  const auto& game_id =
      state != Core::State::Uninitialized ? SConfig::GetInstance().GetGameID() : std::string();
  const auto& game_tdb_id = SConfig::GetInstance().GetGameTDBID();
  const u16 revision = SConfig::GetInstance().GetRevision();

  if (!force && m_game_id == game_id && m_game_tdb_id == game_tdb_id && m_revision == revision)
    return;

  m_game_id = game_id;
  m_game_tdb_id = game_tdb_id;
  m_revision = revision;

  if (m_ar_code)
  {
    const int tab_index = m_tab_widget->indexOf(m_ar_code);
    if (tab_index != -1)
      m_tab_widget->removeTab(tab_index);
    m_ar_code->deleteLater();
    m_ar_code = nullptr;
  }

  if (m_gecko_code)
  {
    const int tab_index = m_tab_widget->indexOf(m_gecko_code);
    if (tab_index != -1)
      m_tab_widget->removeTab(tab_index);
    m_gecko_code->deleteLater();
    m_gecko_code = nullptr;
  }

  if (m_primehack_cheats)
  {
    const int tab_index = m_tab_widget->indexOf(m_primehack_cheats);
    if (tab_index != -1)
      m_tab_widget->removeTab(tab_index);
    m_primehack_cheats->deleteLater();
    m_primehack_cheats = nullptr;
  }

  m_ar_code = new ARCodeWidget(m_game_id, m_revision, false);
  m_gecko_code = new GeckoCodeWidget(m_game_id, m_game_tdb_id, m_revision, false);
  m_primehack_cheats = new PrimeCheatsWidget();
  m_tab_widget->insertTab(0, m_ar_code, tr("AR Code"));
  m_tab_widget->insertTab(1, m_gecko_code, tr("Gecko Codes"));
  m_tab_widget->insertTab(2, m_primehack_cheats, tr("PrimeHack"));
  m_tab_widget->setTabUnclosable(0);
  m_tab_widget->setTabUnclosable(1);
  m_tab_widget->setTabUnclosable(2);

  connect(m_ar_code, &ARCodeWidget::OpenGeneralSettings, this, &CheatsManager::OpenGeneralSettings);
  connect(m_gecko_code, &GeckoCodeWidget::OpenGeneralSettings, this,
          &CheatsManager::OpenGeneralSettings);
}

void CheatsManager::CreateWidgets()
{
  m_tab_widget = new PartiallyClosableTabWidget;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_cheat_search_new = new CheatSearchFactoryWidget();
  m_tab_widget->addTab(m_cheat_search_new, tr("Start New Cheat Search"));
  m_tab_widget->setTabUnclosable(0);

  auto* layout = new QVBoxLayout;
  layout->addWidget(m_tab_widget);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void CheatsManager::OnNewSessionCreated(const Cheats::CheatSearchSessionBase& session)
{
  auto* w = new CheatSearchWidget(session.Clone());
  const int tab_index = m_tab_widget->addTab(w, tr("Cheat Search"));
  w->connect(w, &CheatSearchWidget::ActionReplayCodeGenerated, this,
             [this](const ActionReplay::ARCode& ar_code) {
               if (m_ar_code)
                 m_ar_code->AddCode(ar_code);
             });
  m_tab_widget->setCurrentIndex(tab_index);
}

void CheatsManager::OnTabCloseRequested(int index)
{
  auto* w = m_tab_widget->widget(index);
  if (w)
    w->deleteLater();
}

void CheatsManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_cheat_search_new, &CheatSearchFactoryWidget::NewSessionCreated, this,
          &CheatsManager::OnNewSessionCreated);
  connect(m_tab_widget, &QTabWidget::tabCloseRequested, this, &CheatsManager::OnTabCloseRequested);
}
