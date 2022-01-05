﻿#pragma region std_headers
#include <stdexcept>
#pragma endregion std_headers

#pragma region qt_headers
#include <QVBoxLayout>
#include <QWindow>
#include <QResizeEvent>
#include <QVariant>
#pragma endregion qt_headers

#pragma region cef_headers
#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/cef_frame.h>
#include <include/cef_parser.h>
#pragma endregion cef_headers

#include <CefViewBrowserHandler.h>
#include <CefViewCoreProtocol.h>

#include <QCefQuery.h>
#include <QCefView.h>
#include <QCefEvent.h>

#include "details/CCefManager.h"
#include "details/QCefWindow.h"
#include "details/CCefHandlerDelegate.h"
#include "details/QCefEventPrivate.h"

//////////////////////////////////////////////////////////////////////////
class QCefView::Implementation
{
public:
  /// <summary>
  ///
  /// </summary>
  typedef struct FolderMapping
  {
    QString path;
    QString url;
    int priority;
  } FolderMapping;
  static QList<FolderMapping> folderMappingList_;

  /// <summary>
  ///
  /// </summary>
  typedef struct ArchiveMapping
  {
    QString path;
    QString url;
    QString psw;
  } ArchiveMapping;
  static QList<ArchiveMapping> archiveMappingList_;

private:
  /// <summary>
  ///
  /// </summary>
  CefRefPtr<CefViewBrowserHandler> pQCefViewHandler_;

  /// <summary>
  ///
  /// </summary>
  CefViewBrowserHandlerDelegateInterface::RefPtr pCefHandlerDelegate_;

public:
  explicit Implementation(const QString& url, QCefView* view, QCefWindow* win)
    : pQCefViewHandler_(nullptr)
    , pCefHandlerDelegate_(nullptr)
  {
    // Set window info
    CefWindowInfo window_info;

#if defined(OS_WINDOWS)
    window_info.SetAsChild((CefWindowHandle)(win->winId()), RECT{ 0, 0, 0, 0 });
#elif defined(OS_MACOS)
    window_info.SetAsChild((CefWindowHandle)(win->winId()), 0, 0, 0, 0);
#elif defined(OS_LINUX)
    window_info.SetAsChild((CefWindowHandle)(win->winId()), CefRect{ 0, 0, 0, 0 });
#endif

    CefBrowserSettings browserSettings;
    // disable all plugins
    browserSettings.plugins = STATE_DISABLED;

    // create the delegate
    auto pCefDelegate = std::make_shared<CCefHandlerDelegate>(view, win);

    // create the browser
    auto pQCefViewHandler = new CefViewBrowserHandler(pCefDelegate);

    // add archive mapping
    for (auto archiveMapping : archiveMappingList_) {
      pQCefViewHandler->AddArchiveResourceProvider(
        archiveMapping.path.toStdString(), archiveMapping.url.toStdString(), archiveMapping.psw.toStdString());
    }

    // add local folder mapping
    for (auto folderMapping : folderMappingList_) {
      pQCefViewHandler->AddLocalDirectoryResourceProvider(
        folderMapping.path.toStdString(), folderMapping.url.toStdString(), folderMapping.priority);
    }

    // create the main browser window.
    if (!CefBrowserHost::CreateBrowser(window_info,       // window info
                                       pQCefViewHandler,  // handler
                                       url.toStdString(), // url
                                       browserSettings,   // settings
                                       nullptr,
                                       CefRequestContext::GetGlobalContext())) {
      throw std::exception();
    }

    // hold the browser handler
    CCefManager::getInstance()->registerBrowserHandler(pQCefViewHandler);

    // update members
    pCefHandlerDelegate_ = pCefDelegate;
    pQCefViewHandler_ = pQCefViewHandler;
  }

  ~Implementation()
  {
    if (pQCefViewHandler_) {
      pQCefViewHandler_->CloseAllBrowsers(true);
      CCefManager::getInstance()->removeBrowserHandler(pQCefViewHandler_);
      pQCefViewHandler_ = nullptr;
    }

    if (pCefHandlerDelegate_)
      pCefHandlerDelegate_.reset();
  }

  void closeAllBrowsers()
  {
    if (pQCefViewHandler_) {
      pQCefViewHandler_->CloseAllBrowsers(true);
    }
  }

  void addLocalFolderResource(const QString& path, const QString& url)
  {
    if (pQCefViewHandler_) {
      pQCefViewHandler_->AddLocalDirectoryResourceProvider(path.toStdString(), url.toStdString());
    }
  }

  void addArchiveResource(const QString& path, const QString& url, const QString& password)
  {
    if (pQCefViewHandler_) {
      pQCefViewHandler_->AddArchiveResourceProvider(path.toStdString(), url.toStdString(), password.toStdString());
    }
  }

  void navigateToString(const QString& content)
  {
    if (pQCefViewHandler_) {
      std::string data = content.toStdString();
      data = CefURIEncode(CefBase64Encode(data.c_str(), data.size()), false).ToString();
      data = "data:text/html;base64," + data;
      pQCefViewHandler_->GetBrowser()->GetMainFrame()->LoadURL(data);
    }
  }

  void navigateToUrl(const QString& url)
  {
    if (pQCefViewHandler_) {
      CefString strUrl;
      strUrl.FromString(url.toStdString());
      pQCefViewHandler_->GetBrowser()->GetMainFrame()->LoadURL(strUrl);
    }
  }

  bool browserCanGoBack()
  {
    if (pQCefViewHandler_)
      return pQCefViewHandler_->GetBrowser()->CanGoBack();

    return false;
  }

  bool browserCanGoForward()
  {
    if (pQCefViewHandler_)
      return pQCefViewHandler_->GetBrowser()->CanGoForward();

    return false;
  }

  void browserGoBack()
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->GetBrowser()->GoBack();
  }

  void browserGoForward()
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->GetBrowser()->GoForward();
  }

  bool browserIsLoading()
  {
    if (pQCefViewHandler_)
      return pQCefViewHandler_->GetBrowser()->IsLoading();

    return false;
  }

  void browserReload()
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->GetBrowser()->Reload();
  }

  void browserStopLoad()
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->GetBrowser()->StopLoad();
  }

  bool triggerEvent(const QString& name, const QVariantMap& args, int frameId = CefViewBrowserHandler::MAIN_FRAME)
  {
    if (!name.isEmpty()) {
      return sendEventNotifyMessage(frameId, name, args);
    }

    return false;
  }

  bool responseQCefQuery(const QCefQuery& query)
  {
    if (pQCefViewHandler_) {
      CefString res;
      res.FromString(query.response().toStdString());
      return pQCefViewHandler_->ResponseQuery(query.id(), query.result(), res, query.error());
    }
    return false;
  }

  void notifyMoveOrResizeStarted()
  {
    if (pQCefViewHandler_) {
      CefRefPtr<CefBrowser> browser = pQCefViewHandler_->GetBrowser();
      if (browser) {
        CefRefPtr<CefBrowserHost> host = browser->GetHost();
        if (host)
          host->NotifyMoveOrResizeStarted();
      }
    }
  }

  bool sendEventNotifyMessage(int frameId, const QString& name, const QVariantMap& args)
  {
    CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(TRIGGEREVENT_NOTIFY_MESSAGE);
    CefRefPtr<CefListValue> arguments = msg->GetArgumentList();

    int idx = 0;
    CefString eventName = name.toStdString();
    arguments->SetString(idx++, eventName);

    CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();

    CefString cefStr;
    cefStr.FromString(name.toUtf8().constData());
    dict->SetString("name", cefStr);

    QList<QString> keys = args.keys();
    for (const QString& key : keys) {
      auto value = args[key];
      if (value.type() == QVariant::Bool)
        dict->SetBool(key.toUtf8().constData(), value.toBool());
      else if (value.type() == QVariant::Int || value.type() == QVariant::UInt)
        dict->SetInt(key.toUtf8().constData(), value.toInt());
      else if (value.type() == QVariant::Double)
        dict->SetDouble(key.toUtf8().constData(), value.toDouble());
      else if (value.type() == QVariant::String) {
        cefStr.FromString(value.toString().toUtf8().constData());
        dict->SetString(key.toUtf8().constData(), cefStr);
      } else {
        Q_ASSERT_X(false, "QCefViewPrivate::sendEventNotifyMessage", "Unsupport event argument type");
      }
    }

    arguments->SetDictionary(idx++, dict);

    return pQCefViewHandler_->TriggerEvent(frameId, msg);
  }

  void onToplevelWidgetMoveOrResize() { notifyMoveOrResizeStarted(); }

  void setContextMenuHandler(CefContextMenuHandler* handler)
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->SetContextMenuHandler(handler);
  }

  void setDialogHandler(CefDialogHandler* handler)
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->SetDialogHandler(handler);
  }

  void setDisplayHandler(CefDisplayHandler* handler)
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->SetDisplayHandler(handler);
  }

  void setDownloadHandler(CefDownloadHandler* handler)
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->SetDownloadHandler(handler);
  }

  void setJSDialogHandler(CefJSDialogHandler* handler)
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->SetJSDialogHandler(handler);
  }

  void setKeyboardHandler(CefKeyboardHandler* handler)
  {
    if (pQCefViewHandler_)
      pQCefViewHandler_->SetKeyboardHandler(handler);
  }
};

QList<QCefView::Implementation::FolderMapping> QCefView::Implementation::folderMappingList_;
QList<QCefView::Implementation::ArchiveMapping> QCefView::Implementation::archiveMappingList_;

QCefView::QCefView(const QString url, QWidget* parent /*= 0*/)
  : QWidget(parent)
  , pImpl_(nullptr)
{
  // initialize the layout
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  // create the window
  QCefWindow* pCefWindow = new QCefWindow(windowHandle(), this);
  pCefWindow->create();

  // create window container
  QWidget* windowContainer = createWindowContainer(pCefWindow, this);
  layout->addWidget(windowContainer);

  // create the implementation
  pImpl_ = std::unique_ptr<Implementation>(new Implementation(url, this, pCefWindow));

  // If we're already part of a window, we'll install our event handler
  // If our parent changes later, this will be handled in QCefView::changeEvent()
  if (this->window())
    this->window()->installEventFilter(this);
}

QCefView::QCefView(QWidget* parent /*= 0*/)
  : QCefView("about:blank", parent)
{}

QCefView::~QCefView()
{
  disconnect();

  if (pImpl_)
    pImpl_.reset();
}

void
QCefView::addLocalFolderResource(const QString& path, const QString& url, int priority /* = 0*/)
{
  Implementation::folderMappingList_.push_back({ path, url, priority });
}

void
QCefView::addArchiveResource(const QString& path, const QString& url, const QString& password /* = ""*/)
{
  Implementation::archiveMappingList_.push_back({ path, url, password });
}

void
QCefView::addCookie(const QString& name, const QString& value, const QString& domain, const QString& url)
{
  CCefManager::getInstance()->addCookie(
    name.toStdString(), value.toStdString(), domain.toStdString(), url.toStdString());
}

void
QCefView::navigateToString(const QString& content)
{
  if (pImpl_)
    pImpl_->navigateToString(content);
}

void
QCefView::navigateToUrl(const QString& url)
{
  if (pImpl_)
    pImpl_->navigateToUrl(url);
}

bool
QCefView::browserCanGoBack()
{
  if (pImpl_)
    return pImpl_->browserCanGoBack();

  return false;
}

bool
QCefView::browserCanGoForward()
{
  if (pImpl_)
    return pImpl_->browserCanGoForward();

  return false;
}

void
QCefView::browserGoBack()
{
  if (pImpl_)
    pImpl_->browserGoBack();
}

void
QCefView::browserGoForward()
{
  if (pImpl_)
    pImpl_->browserGoForward();
}

bool
QCefView::browserIsLoading()
{
  if (pImpl_)
    return pImpl_->browserIsLoading();

  return false;
}

void
QCefView::browserReload()
{
  if (pImpl_)
    pImpl_->browserReload();
}

void
QCefView::browserStopLoad()
{
  if (pImpl_)
    pImpl_->browserStopLoad();
}

bool
QCefView::triggerEvent(const QCefEvent& event)
{
  if (pImpl_)
    return pImpl_->triggerEvent(event.eventName(), event.d_func()->args, CefViewBrowserHandler::MAIN_FRAME);

  return false;
}

bool
QCefView::triggerEvent(const QCefEvent& event, int frameId)
{
  if (pImpl_)
    return pImpl_->triggerEvent(event.eventName(), event.d_func()->args, frameId);

  return false;
}

bool
QCefView::broadcastEvent(const QCefEvent& event)
{
  if (pImpl_)
    return pImpl_->triggerEvent(event.eventName(), event.d_func()->args, CefViewBrowserHandler::ALL_FRAMES);

  return false;
}

bool
QCefView::responseQCefQuery(const QCefQuery& query)
{
  if (pImpl_)
    return pImpl_->responseQCefQuery(query);

  return false;
}

void
QCefView::setContextMenuHandler(CefContextMenuHandler* handler)
{
  if (pImpl_)
    return pImpl_->setContextMenuHandler(handler);
}

void
QCefView::setDialogHandler(CefDialogHandler* handler)
{
  if (pImpl_)
    return pImpl_->setDialogHandler(handler);
}

void
QCefView::setDisplayHandler(CefDisplayHandler* handler)
{
  if (pImpl_)
    return pImpl_->setDisplayHandler(handler);
}

void
QCefView::setDownloadHandler(CefDownloadHandler* handler)
{
  if (pImpl_)
    return pImpl_->setDownloadHandler(handler);
}

void
QCefView::setJSDialogHandler(CefJSDialogHandler* handler)
{
  if (pImpl_)
    return pImpl_->setJSDialogHandler(handler);
}

void
QCefView::setKeyboardHandler(CefKeyboardHandler* handler)
{
  if (pImpl_)
    return pImpl_->setKeyboardHandler(handler);
}

void
QCefView::onLoadingStateChanged(bool isLoading, bool canGoBack, bool canGoForward)
{}

void
QCefView::onLoadStart()
{}

void
QCefView::onLoadEnd(int httpStatusCode)
{}

void
QCefView::onLoadError(int errorCode, const QString& errorMsg, const QString& failedUrl, bool& handled)
{}

void
QCefView::onDraggableRegionChanged(const QRegion& draggableRegion, const QRegion& nonDraggableRegion)
{}

void
QCefView::onConsoleMessage(const QString& message, int level)
{}

void
QCefView::onTakeFocus(bool next)
{}

void
QCefView::onQCefUrlRequest(const QString& url)
{}

void
QCefView::onQCefQueryRequest(const QCefQuery& query)
{}

void
QCefView::onInvokeMethodNotify(int browserId, int frameId, const QString& method, const QVariantList& arguments)
{}

void
QCefView::changeEvent(QEvent* event)
{
  if (QEvent::ParentAboutToChange == event->type()) {
    if (this->window())
      this->window()->removeEventFilter(this);
  } else if (QEvent::ParentChange == event->type()) {
    if (this->window())
      this->window()->installEventFilter(this);
  }
  QWidget::changeEvent(event);
}

bool
QCefView::eventFilter(QObject* watched, QEvent* event)
{
  if (!pImpl_)
    return QWidget::eventFilter(watched, event);

  if (watched == this->window()) {
    if (QEvent::Resize == event->type() || QEvent::Move == event->type()) {
      pImpl_->onToplevelWidgetMoveOrResize();
    }
  }

  if (watched == this) {
    if (QEvent::MouseButtonPress == event->type()) {
      QMouseEvent* e = (QMouseEvent*)event;
      if (e->button() == Qt::LeftButton) {
        bIsDragging_ = true;
        mLastMousePosition_ = e->pos();
      }
    }
  }

  return QWidget::eventFilter(watched, event);
}

void
QCefView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton) {
    bIsDragging_ = true;
    mLastMousePosition_ = event->pos();
  }
  QWidget::mousePressEvent(event);
}

void
QCefView::mouseMoveEvent(QMouseEvent* event)
{
  if (event->buttons().testFlag(Qt::LeftButton) && bIsDragging_) {
    this->move(this->pos() + (event->pos() - mLastMousePosition_));
    mLastMousePosition_ = event->pos();
  }
  QWidget::mouseMoveEvent(event);
}

void
QCefView::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton) {
    bIsDragging_ = false;
  }
  QWidget::mouseReleaseEvent(event);
}
