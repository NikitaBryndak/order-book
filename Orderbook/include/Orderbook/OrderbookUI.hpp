#pragma once
/* ==========================================================================
   OrderbookUI.hpp  –  Single-file Qt6 UI for the order-book simulation.

   ALL Orderbook UI member-function implementations live here so that
   Orderbook.cpp stays a pure, zero-overhead matching engine.
   ========================================================================== */

#ifndef OB_ENABLE_UI
#error "OrderbookUI.hpp requires OB_ENABLE_UI to be defined"
#endif

// ── Core headers ────────────────────────────────────────────────────────────
#include "Orderbook.hpp"
#include "Config.hpp"

// ── Qt headers ──────────────────────────────────────────────────────────────
#include <QApplication>
#include <QBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

// ── STL ─────────────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

/* ==========================================================================
   Color palette
   ========================================================================== */
namespace OBColors {
  inline constexpr auto kBg          = "#111428";
  inline constexpr auto kPanel       = "#171a32";
  inline constexpr auto kPanelSoft   = "#1c2040";
  inline constexpr auto kPanelRaised = "#23294d";
  inline constexpr auto kBorder      = "#333c6d";
  inline constexpr auto kText        = "#d7dcff";
  inline constexpr auto kDimText     = "#8f98c9";
  inline constexpr auto kGreen       = "#83f2a8";
  inline constexpr auto kRed         = "#ff8fb1";
  inline constexpr auto kAccent      = "#66b6ff";
  inline constexpr auto kGold        = "#ffd37f";
  inline constexpr auto kGreenBar    = "#4d83f2a8";
  inline constexpr auto kRedBar      = "#4dff8fb1";
}

/* ==========================================================================
   DepthWidget  –  bid / ask depth ladder
   ========================================================================== */
class DepthWidget : public QWidget {
  Q_OBJECT
public:
  explicit DepthWidget(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(340, 300);
  }

  void setSnapshot(const OrderBookSnapshot& s) { snap_ = s; update(); }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(OBColors::kPanel));

    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor("#1b1f3f"));
    bg.setColorAt(1.0, QColor(OBColors::kPanel));
    p.fillRect(rect().adjusted(1, 1, -1, -1), bg);

    p.setPen(QPen(QColor(OBColors::kBorder), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    const int headerH = 30;
    const int rowH    = 22;
    const int midY    = height() / 2;
    const int levels  = std::max<int>(snap_.bidLevels.size(),
                                      snap_.askLevels.size());
    if (levels == 0) {
      p.setPen(QColor(OBColors::kDimText));
      p.drawText(rect(), Qt::AlignCenter, "Waiting for data...");
      return;
    }

    p.setPen(QColor(OBColors::kText));
    p.drawText(QRect(12, 8, width() - 24, 16), Qt::AlignLeft | Qt::AlignVCenter,
               "Depth Ladder");
    p.setPen(QColor(OBColors::kDimText));
    p.drawText(QRect(12, 8, width() - 24, 16), Qt::AlignRight | Qt::AlignVCenter,
               QString("Levels: %1").arg(levels));

    p.setPen(QPen(QColor(OBColors::kBorder), 1));
    p.drawLine(10, headerH, width() - 10, headerH);

    Quantity maxQty = 1;
    for (auto& [pr, q] : snap_.askLevels) maxQty = std::max(maxQty, q);
    for (auto& [pr, q] : snap_.bidLevels) maxQty = std::max(maxQty, q);

    auto drawRow = [&](int idx, Price price, Quantity qty,
                       bool isBid, int startY, int dir) {
      int y = startY + dir * idx * rowH;
      double pct = double(qty) / maxQty;
      double eased = std::sqrt(std::max(0.0, pct));
      int barW = int(eased * (width() - 175));

      QRect barRect(width() - 10 - barW, y, barW, rowH - 2);
      p.fillRect(barRect, QColor(isBid ? OBColors::kGreenBar
                                       : OBColors::kRedBar));

      p.setPen(QColor("#2b335d"));
      p.drawRect(barRect);

      p.setPen(QColor(isBid ? OBColors::kGreen : OBColors::kRed));
      QString txt = QString("%1  x%2").arg(price).arg(qty);
      p.drawText(QRect(10, y, width() - 20, rowH - 2),
                 Qt::AlignVCenter | Qt::AlignLeft, txt);
    };

    int askCount = std::min<int>(levels, snap_.askLevels.size());
    for (int i = 0; i < askCount; ++i)
      drawRow(i, snap_.askLevels[askCount - 1 - i].first,
              snap_.askLevels[askCount - 1 - i].second,
              false, midY - rowH - 2, -1);

    int bidCount = std::min<int>(levels, snap_.bidLevels.size());
    for (int i = 0; i < bidCount; ++i)
      drawRow(i, snap_.bidLevels[i].first, snap_.bidLevels[i].second,
              true, midY + 3, 1);

    // spread line
    p.setPen(QPen(QColor(OBColors::kAccent), 1, Qt::DashLine));
    p.drawLine(0, midY, width(), midY);
    p.setPen(QColor(OBColors::kDimText));
    p.drawText(QRect(12, midY - 10, width() - 24, 20), Qt::AlignRight | Qt::AlignVCenter,
               "spread");
  }

private:
  OrderBookSnapshot snap_;
};

/* ==========================================================================
   CandlestickWidget  –  OHLC price chart
   ========================================================================== */
class CandlestickWidget : public QWidget {
  Q_OBJECT
public:
  explicit CandlestickWidget(QWidget* parent = nullptr) : QWidget(parent),
      candleWindow_(500) {
    setMinimumSize(400, 260);
  }

  void setSnapshot(const OrderBookSnapshot& s) { snap_ = s; update(); }
  void setCandleWindow(int window) { candleWindow_ = window; update(); }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(OBColors::kPanel));

    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor("#1a1f3c"));
    bg.setColorAt(1.0, QColor(OBColors::kPanel));
    p.fillRect(rect().adjusted(1, 1, -1, -1), bg);

    p.setPen(QPen(QColor(OBColors::kBorder), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    if (snap_.candles.empty()) {
      p.setPen(QColor(OBColors::kDimText));
      p.drawText(rect(), Qt::AlignCenter, "Accumulating candles...");
      return;
    }

    // Get the last N candles based on window size
    int startIdx = std::max(0, (int)snap_.candles.size() - candleWindow_);
    std::vector<Candlestick> windowCandles(
        snap_.candles.begin() + startIdx,
        snap_.candles.end());

    const int pad  = 46;
    const int topPad = 30;
    const int bottomPad = 38;
    const int chartH = std::max(80, height() - topPad - bottomPad);

    const int cw   = std::max(3, (width() - 2 * pad) /
                                  (int)windowCandles.size() - 2);
    Price lo = UINT64_MAX, hi = 0;
    for (auto& c : windowCandles) {
      lo = std::min(lo, c.low);
      hi = std::max(hi, c.high);
    }
    if (hi == lo) hi = lo + 1;

    auto yOf = [&](Price pr) -> int {
      return topPad + int(double(hi - pr) / (hi - lo) * chartH);
    };

    auto volY = [&](double pct) -> int {
      return topPad + chartH + 4 + int((1.0 - pct) * (bottomPad - 8));
    };

    // grid
    p.setPen(QColor(OBColors::kText));
    p.drawText(QRect(12, 8, width() - 24, 16), Qt::AlignLeft | Qt::AlignVCenter,
               "Candlestick Chart");
    p.setPen(QColor(OBColors::kDimText));
    p.drawText(QRect(12, 8, width() - 24, 16), Qt::AlignRight | Qt::AlignVCenter,
               QString("Window: %1").arg((int)windowCandles.size()));

    p.setPen(QColor(OBColors::kBorder));
    for (int i = 0; i <= 4; ++i) {
      int gy = topPad + i * chartH / 4;
      p.drawLine(pad, gy, width() - 10, gy);
      Price gp = hi - (hi - lo) * i / 4;
      p.setPen(QColor(OBColors::kDimText));
      p.drawText(0, gy - 6, pad - 4, 12, Qt::AlignRight,
                 QString::number(gp));
      p.setPen(QColor(OBColors::kBorder));
    }

    uint64_t maxVol = 1;
    for (auto& c : windowCandles) maxVol = std::max(maxVol, c.volume);

    int x = pad;
    QVector<QPoint> maPoints;
    const int maLen = 10;

    for (auto& c : windowCandles) {
      bool bull = c.isBullish();
      QColor col(bull ? OBColors::kGreen : OBColors::kRed);

      int yHigh  = yOf(c.high);
      int yLow   = yOf(c.low);
      int yOpen  = yOf(c.open);
      int yClose = yOf(c.close);

      // wick
      p.setPen(col);
      int cx = x + cw / 2;
      p.drawLine(cx, yHigh, cx, yLow);

      // body
      int bodyTop = std::min(yOpen, yClose);
      int bodyH   = std::max(1, std::abs(yOpen - yClose));
      p.fillRect(QRect(x, bodyTop, cw, bodyH), col);

      double vPct = double(c.volume) / double(maxVol);
      int vTop = volY(vPct);
      p.fillRect(QRect(x, vTop, cw, topPad + chartH + bottomPad - 4 - vTop),
                 QColor(bull ? OBColors::kGreenBar : OBColors::kRedBar));

      if ((int)maPoints.size() + 1 >= maLen) {
        uint64_t sum = 0;
        int begin = std::max(0, (int)maPoints.size() + 1 - maLen);
        for (int i = begin; i < (int)maPoints.size(); ++i) {
          int src = i - begin;
          sum += windowCandles[begin + src].close;
        }
        sum += c.close;
        Price avg = sum / maLen;
        maPoints.push_back(QPoint(x + cw / 2, yOf(avg)));
      } else {
        maPoints.push_back(QPoint(x + cw / 2, yOf(c.close)));
      }

      x += cw + 2;
    }

    p.setPen(QPen(QColor(OBColors::kAccent), 2));
    for (int i = 1; i < maPoints.size(); ++i) {
      p.drawLine(maPoints[i - 1], maPoints[i]);
    }

    const auto& last = windowCandles.back();
    int yLast = yOf(last.close);
    p.setPen(QPen(QColor(OBColors::kGold), 1, Qt::DashLine));
    p.drawLine(pad, yLast, width() - 10, yLast);
    p.setPen(QColor(OBColors::kGold));
    p.drawText(QRect(10, yLast - 10, width() - 20, 18), Qt::AlignRight | Qt::AlignVCenter,
               QString("last: %1").arg(last.close));
  }

private:
  OrderBookSnapshot snap_;
  int candleWindow_;
};

/* ==========================================================================
   StatsPanel  –  top bar with key numbers
   ========================================================================== */
class StatsPanel : public QWidget {
  Q_OBJECT
public:
  explicit StatsPanel(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedHeight(52);
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(8);

    auto mk = [&](const QString& lbl) {
      auto* l = new QLabel(lbl, this);
      l->setAlignment(Qt::AlignCenter);
      l->setStyleSheet(QString("color:%1; font-size:13px; background:%2; border:1px solid %3; border-radius:8px; padding:5px 10px;")
                           .arg(OBColors::kText, OBColors::kPanelRaised, OBColors::kBorder));
      lay->addWidget(l);
      return l;
    };

    bidLabel_    = mk("Bid: ---");
    askLabel_    = mk("Ask: ---");
    spreadLabel_ = mk("Spread: ---");
    orderLabel_  = mk("Orders: 0");
    matchLabel_  = mk("Matches: 0");

    setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                      .arg(OBColors::kPanel, OBColors::kBorder));
  }

  void setSnapshot(const OrderBookSnapshot& s) {
    uint64_t bid = s.topBid;
    uint64_t ask = s.topAsk;
    uint64_t spr = (s.topAsk > s.topBid) ? (s.topAsk - s.topBid) : 0;

    bidLabel_->setText(QString("Bid: <span style='color:%1'>%2</span>")
                          .arg(OBColors::kGreen)
                          .arg(bid));
    bidLabel_->setTextFormat(Qt::RichText);

    askLabel_->setText(QString("Ask: <span style='color:%1'>%2</span>")
                          .arg(OBColors::kRed)
                          .arg(ask));
    askLabel_->setTextFormat(Qt::RichText);

    spreadLabel_->setText(QString("Spread: %1").arg(spr));
    orderLabel_->setText(QString("Orders: %1").arg(s.orderCount));
    matchLabel_->setText(QString("Matches: %1").arg(s.matchCount));
  }

private:
  QLabel* bidLabel_;
  QLabel* askLabel_;
  QLabel* spreadLabel_;
  QLabel* orderLabel_;
  QLabel* matchLabel_;
};

/* ==========================================================================
   MarketViewWindow  –  expanded market view with larger candlestick chart
   ========================================================================== */
class MarketViewWindow : public QWidget {
  Q_OBJECT
public:
  explicit MarketViewWindow(Orderbook& ob, QWidget* parent = nullptr)
      : QWidget(parent), ob_(ob) {
    setWindowTitle("Market View");
    resize(1400, 700);
    setStyleSheet(QString("background:%1; color:%2;").arg(OBColors::kBg, OBColors::kText));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stats_ = new StatsPanel(this);
    root->addWidget(stats_);

    // Candle window controls
    auto* ctrlLayout = new QHBoxLayout;
    ctrlLayout->setContentsMargins(6, 6, 6, 0);
    ctrlLayout->setSpacing(4);
    auto* lbl = new QLabel("Candle Window", this);
    lbl->setStyleSheet(QString("color:%1; font-size:13px;").arg(OBColors::kDimText));
    ctrlLayout->addWidget(lbl);

    int windows[] = {20, 50, 100, 200, 500};
    for (int w : windows) {
      auto* btn = new QPushButton(QString::number(w), this);
      btn->setMaximumWidth(60);
      if (w == 500) btn->setStyleSheet(QString("background:%1; color:#08131a; border:1px solid %2; border-radius:8px; padding:4px 10px;")
                                       .arg(OBColors::kGreen, OBColors::kBorder));
      connect(btn, &QPushButton::clicked, this, [this, w]() {
        candle_->setCandleWindow(w);
        updateButtonStates(w);
      });
      candleButtons_[w] = btn;
      ctrlLayout->addWidget(btn);
    }

    ctrlLayout->addStretch();
    root->addLayout(ctrlLayout);

    candle_ = new CandlestickWidget(this);
    root->addWidget(candle_, 1);

    // 10 Hz refresh
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MarketViewWindow::refresh);
    timer->start(100);
  }

private slots:
  void refresh() {
    OrderRequest req;
    req.type = RequestType::Snapshot;
    ob_.submitRequest(req);

    OrderBookSnapshot snap = ob_.getSnapshot();
    stats_->setSnapshot(snap);
    candle_->setSnapshot(snap);
  }

  void updateButtonStates(int activeWindow) {
    for (auto& [w, btn] : candleButtons_) {
      if (w == activeWindow) {
        btn->setStyleSheet(QString("background:%1; color:#08131a; border:1px solid %2; border-radius:8px; padding:4px 10px;")
                               .arg(OBColors::kGreen, OBColors::kBorder));
      } else {
        btn->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:4px 10px;")
                               .arg(OBColors::kPanelSoft, OBColors::kText, OBColors::kBorder));
      }
    }
  }

private:
  Orderbook& ob_;
  StatsPanel*        stats_;
  CandlestickWidget* candle_;
  std::map<int, QPushButton*> candleButtons_;
};

/* ==========================================================================
   OrderBookWindow  –  main window tying everything together
   ========================================================================== */
class OrderBookWindow : public QWidget {
  Q_OBJECT
public:
  explicit OrderBookWindow(Orderbook& ob, QWidget* parent = nullptr)
      : QWidget(parent), ob_(ob) {
    setWindowTitle("Order Book");
    resize(900, 600);
    setStyleSheet(QString(
        "background:%1;"
        "QPushButton {"
        " background:%2; color:%3; border:1px solid %4; border-radius:8px; padding:6px 12px;"
        "}"
        "QPushButton:hover { background:%5; }"
        "QPushButton:pressed { background:%4; }"
    ).arg(OBColors::kBg, OBColors::kPanelSoft, OBColors::kText, OBColors::kBorder, OBColors::kPanelRaised));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stats_ = new StatsPanel(this);
    root->addWidget(stats_);

    auto* body = new QHBoxLayout;
    body->setSpacing(6);
    body->setContentsMargins(6, 6, 6, 6);

    depth_  = new DepthWidget(this);
    candle_ = new CandlestickWidget(this);

    body->addWidget(depth_,  1);
    body->addWidget(candle_, 2);
    root->addLayout(body);

    // Market View button
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(6, 6, 6, 6);
    btnLayout->addStretch();
    auto* btnMarket = new QPushButton("Market View", this);
    btnMarket->setMinimumHeight(30);
    connect(btnMarket, &QPushButton::clicked, this, &OrderBookWindow::openMarketView);
    btnLayout->addWidget(btnMarket);
    root->addLayout(btnLayout);

    // 10 Hz refresh
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &OrderBookWindow::refresh);
    timer->start(100);
  }

private slots:
  void refresh() {
    // Request a fresh snapshot from the worker thread
    OrderRequest req;
    req.type = RequestType::Snapshot;
    ob_.submitRequest(req);

    OrderBookSnapshot snap = ob_.getSnapshot();
    stats_->setSnapshot(snap);
    depth_->setSnapshot(snap);
    candle_->setSnapshot(snap);
  }

  void openMarketView() {
    if (!marketWindow_) {
      marketWindow_ = new MarketViewWindow(ob_);
    }
    marketWindow_->show();
    marketWindow_->raise();
    marketWindow_->activateWindow();
  }

private:
  Orderbook& ob_;
  StatsPanel*        stats_;
  DepthWidget*       depth_;
  CandlestickWidget* candle_;
  MarketViewWindow*  marketWindow_ = nullptr;
};
