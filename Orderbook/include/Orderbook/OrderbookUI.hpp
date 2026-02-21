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
  inline constexpr auto kBg        = "#1e1e2e";
  inline constexpr auto kPanel     = "#181825";
  inline constexpr auto kBorder    = "#313244";
  inline constexpr auto kText      = "#cdd6f4";
  inline constexpr auto kDimText   = "#6c7086";
  inline constexpr auto kGreen     = "#a6e3a1";
  inline constexpr auto kRed       = "#f38ba8";
  inline constexpr auto kGreenBar  = "#40a6e3a1";
  inline constexpr auto kRedBar    = "#40f38ba8";
  inline constexpr auto kGreenDark = "#2d5a3a";
  inline constexpr auto kRedDark   = "#5a2d3a";
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

    const int rowH   = 22;
    const int midY   = height() / 2;
    const int levels = std::min<int>(12, std::max(snap_.bidLevels.size(),
                                                   snap_.askLevels.size()));
    if (levels == 0) {
      p.setPen(QColor(OBColors::kDimText));
      p.drawText(rect(), Qt::AlignCenter, "Waiting for data...");
      return;
    }

    Quantity maxQty = 1;
    for (auto& [pr, q] : snap_.askLevels) maxQty = std::max(maxQty, q);
    for (auto& [pr, q] : snap_.bidLevels) maxQty = std::max(maxQty, q);

    auto drawRow = [&](int idx, Price price, Quantity qty,
                       bool isBid, int startY, int dir) {
      int y = startY + dir * idx * rowH;
      double pct = double(qty) / maxQty;
      int barW = int(pct * (width() - 160));

      QRect barRect(width() - 10 - barW, y, barW, rowH - 2);
      p.fillRect(barRect, QColor(isBid ? OBColors::kGreenBar
                                       : OBColors::kRedBar));
      p.setPen(QColor(isBid ? OBColors::kGreen : OBColors::kRed));
      QString txt = QString("%1  x%2")
                        .arg(price)
                        .arg(qty);
      p.drawText(QRect(10, y, width() - 20, rowH - 2),
                 Qt::AlignVCenter | Qt::AlignLeft, txt);
    };

    int askCount = std::min<int>(levels, snap_.askLevels.size());
    for (int i = 0; i < askCount; ++i)
      drawRow(i, snap_.askLevels[askCount - 1 - i].first,
              snap_.askLevels[askCount - 1 - i].second,
              false, midY - rowH, -1);

    int bidCount = std::min<int>(levels, snap_.bidLevels.size());
    for (int i = 0; i < bidCount; ++i)
      drawRow(i, snap_.bidLevels[i].first, snap_.bidLevels[i].second,
              true, midY + 2, 1);

    // spread line
    p.setPen(QPen(QColor(OBColors::kBorder), 1, Qt::DashLine));
    p.drawLine(0, midY, width(), midY);
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

    const int pad  = 40;
    const int cw   = std::max(4, (width() - 2 * pad) /
                                  (int)windowCandles.size() - 2);
    Price lo = UINT64_MAX, hi = 0;
    for (auto& c : windowCandles) {
      lo = std::min(lo, c.low);
      hi = std::max(hi, c.high);
    }
    if (hi == lo) hi = lo + 1;

    auto yOf = [&](Price pr) -> int {
      return pad + int(double(hi - pr) / (hi - lo) * (height() - 2 * pad));
    };

    // grid
    p.setPen(QColor(OBColors::kBorder));
    for (int i = 0; i <= 4; ++i) {
      int gy = pad + i * (height() - 2 * pad) / 4;
      p.drawLine(pad, gy, width() - 10, gy);
      Price gp = hi - (hi - lo) * i / 4;
      p.setPen(QColor(OBColors::kDimText));
      p.drawText(0, gy - 6, pad - 4, 12, Qt::AlignRight,
                 QString::number(gp));
      p.setPen(QColor(OBColors::kBorder));
    }

    int x = pad;
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

      x += cw + 2;
    }
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
    setFixedHeight(38);
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 4, 10, 4);

    auto mk = [&](const QString& lbl) {
      auto* l = new QLabel(lbl, this);
      l->setStyleSheet(QString("color:%1; font-size:13px;")
                           .arg(OBColors::kText));
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
    setStyleSheet(QString("background:%1;").arg(OBColors::kBg));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stats_ = new StatsPanel(this);
    root->addWidget(stats_);

    // Candle window controls
    auto* ctrlLayout = new QHBoxLayout;
    ctrlLayout->setContentsMargins(6, 6, 6, 0);
    ctrlLayout->setSpacing(4);
    ctrlLayout->addWidget(new QLabel("Candles:", this));

    int windows[] = {20, 50, 100, 200, 500};
    for (int w : windows) {
      auto* btn = new QPushButton(QString::number(w), this);
      btn->setMaximumWidth(60);
      if (w == 500) btn->setStyleSheet("background: #a6e3a1; color: black;");
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
        btn->setStyleSheet("background: #a6e3a1; color: black;");
      } else {
        btn->setStyleSheet("");
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
    setStyleSheet(QString("background:%1;").arg(OBColors::kBg));

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
