﻿#include <QCefContext.h>

#include <QTimer>
#include <QDebug>

#include "details/CCefManager.h"

/// <summary>
///
/// </summary>
class QCefContextPrivate
{
public:
  QCefContextPrivate() {}

  QTimer* cefWorkerTimer = nullptr;
  CCefManager::RefPtr pCefManager;
};

QCefContext* QCefContext::s_self;

QCefContext::QCefContext(QCoreApplication* app, const QCefSetting* settings)
  : QObject(app)
  , d_ptr(new QCefContextPrivate)
{
  init(app, settings);
}

QCefContext*
QCefContext::instance()
{
  return s_self;
}

QCefContext::~QCefContext()
{
  uninit();
}

void
QCefContext::scheduleMessageLoopWork(int64_t delay_ms)
{
  if (delay_ms <= 0) {
    QTimer::singleShot(0, this, SLOT(doCefWork()));
  } else {
    QTimer::singleShot(delay_ms, this, SLOT(doCefWork()));
  }
}

void
QCefContext::doCefWork()
{
  Q_D(QCefContext);
  d->pCefManager->doCefWork();
}

bool
QCefContext::init(QObject* parent, const QCefSetting* settings)
{
  Q_ASSERT_X(s_self == nullptr, "QCefContext::init()", "There can be only one QCefContext instance");
  s_self = this;

  Q_D(QCefContext);

  // create and initialize the cef manager
  d->pCefManager = std::make_shared<CCefManager>();
  d->pCefManager->initialize(settings->d_func());

  // create and initialize the worker timer
  d->cefWorkerTimer = new QTimer(parent);
  connect(d->cefWorkerTimer, SIGNAL(timeout()), this, SLOT(doCefWork()));
  d->cefWorkerTimer->setInterval(1000 / 60);
  d->cefWorkerTimer->start();

  return true;
}

void
QCefContext::uninit()
{
  Q_D(QCefContext);

  // reset to release the cef manager
  d->pCefManager->uninitialize();
  d->pCefManager.reset();

  s_self = nullptr;
}
