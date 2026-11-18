#include "ui/settings_window/theme_preview.hpp"

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <utility>
#include <vector>

#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kAspect = 0.56;
constexpr int kMaxHeight = 300;
constexpr qreal kSplitTop = 0.62;
constexpr qreal kSplitBottom = 0.38;

using Regions = std::vector<std::pair<QString, QRectF>>;

class Canvas {
public:
    Canvas(QPainter &painter, const Theme &theme, qreal unit, Regions *regions)
        : p(painter), t(theme), u(unit), m_regions(regions)
    {
    }

    void mark(const char *id, const QRectF &rect) const
    {
        if (m_regions) {
            m_regions->emplace_back(QString::fromLatin1(id), rect);
        }
    }

    void bar(const QRectF &rect, const QColor &color) const { p.fillPath(roundedPath(rect, rect.height() / 2), color); }

    void box(const QRectF &rect, qreal radius, const QColor &color) const { p.fillPath(roundedPath(rect, radius), color); }

    void outline(const QRectF &rect, qreal radius, const QColor &color, qreal width) const
    {
        p.save();
        p.setPen(QPen(color, width));
        p.setBrush(Qt::NoBrush);
        p.drawPath(roundedPath(rect.adjusted(width / 2, width / 2, -width / 2, -width / 2), radius));
        p.restore();
    }

    void dot(const QPointF &center, qreal radius, const QColor &color) const
    {
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(center, radius, radius);
        p.restore();
    }

    QPainter &p;
    const Theme &t;
    qreal u;

private:
    Regions *m_regions;
};

QRectF titleStrip(const Canvas &c, const QRectF &rect)
{
    const QRectF strip(rect.left(), rect.top(), rect.width(), c.u * 3.4);
    c.p.fillRect(strip, c.t.titleBarFill);
    c.mark("title.fill", strip);
    c.box(QRectF(strip.left() + c.u * 1.2, strip.center().y() - c.u * 0.8, c.u * 1.6, c.u * 1.6), c.u * 0.4,
          c.t.accent.blue600);
    const QRectF text(strip.left() + c.u * 3.6, strip.center().y() - c.u * 0.45, strip.width() * 0.22, c.u * 0.9);
    c.bar(text, c.t.titleBarText);
    c.mark("title.text", text);
    for (int i = 0; i < 3; ++i) {
        const QPointF center(strip.right() - c.u * (1.8 + i * 3.0), strip.center().y());
        c.bar(QRectF(center.x() - c.u * 0.6, center.y() - c.u * 0.12, c.u * 1.2, c.u * 0.24),
              withAlpha(c.t.titleBarText, 0.8));
    }
    return QRectF(rect.left(), strip.bottom(), rect.width(), rect.bottom() - strip.bottom());
}

void paintCard(const Canvas &c, const QRectF &card, bool first)
{
    const qreal u = c.u;
    const qreal radius = u * 1.6;
    const qreal edge = c.t.cardTopEdgeHeight > 1 ? std::max<qreal>(1.5, u * 0.5) : std::max<qreal>(1, u * 0.15);
    paintCardSurface(c.p, card, radius, c.t.cardFill, c.t.cardTopEdge, qRound(edge));
    c.mark("card.fill", card);
    c.mark("card.edge", QRectF(card.left(), card.top(), card.width(), std::max<qreal>(edge, u * 0.6)));
    const qreal left = card.left() + u * 2.2;
    const qreal line = u * 1.1;
    qreal top = card.top() + edge + u * 1.6;
    const QRectF title(left, top, card.width() * 0.4, line * 1.3);
    c.bar(title, c.t.cardText);
    c.mark("card.text", title);
    c.mark("app.text", title);
    if (first) {
        const QRectF badge(title.right() + u, title.top() - u * 0.15, u * 4, line * 1.6);
        c.bar(badge, c.t.holdBadgeFill);
        c.bar(QRectF(badge.left() + u * 0.9, badge.center().y() - u * 0.3, badge.width() - u * 1.8, u * 0.6),
              c.t.cardText);
        c.mark("card.hold", badge);
    }
    top += line * 1.3 + u * 1.3;
    const QColor icons[] = {c.t.authorIcon, c.t.transcriberIcon, c.t.bpmIcon};
    const char *iconIds[] = {"card.author", "card.transcriber", "card.bpm"};
    for (int i = 0; i < 3; ++i) {
        const QRectF icon(left, top, line, line);
        c.dot(icon.center(), line / 2, icons[i]);
        c.mark(iconIds[i], icon);
        const QRectF labelBar(left + line * 1.6, top + line * 0.2, card.width() * 0.16, line * 0.6);
        c.bar(labelBar, withAlpha(c.t.cardLabelTint, 0.85));
        c.mark("card.label", labelBar);
        const QRectF valueBar(labelBar.right() + card.width() * 0.02, top + line * 0.2, card.width() * 0.2, line * 0.6);
        c.bar(valueBar, withAlpha(c.t.cardText, 0.85));
        c.mark("card.text", valueBar);
        top += line + u * 0.9;
    }
    const qreal dotSize = u * 0.45;
    const qreal dotsX = card.right() - u * 3;
    for (int i = 0; i < 3; ++i) {
        c.dot(QPointF(dotsX, card.top() + u * 2.6 + i * dotSize * 2.6), dotSize / 2, c.t.actionGlyph);
    }
    c.mark("card.menu", QRectF(dotsX - u, card.top() + u * 1.8, u * 2, dotSize * 6.6));
    if (!first) {
        const QRectF heart(dotsX - u * 4.4, card.top() + u * 1.9, u * 1.6, u * 1.6);
        c.dot(heart.center(), heart.width() / 2, c.t.heartOn);
        c.mark("card.favorite", heart);
    }
}

void paintFooter(const Canvas &c, const QRectF &footer)
{
    const qreal u = c.u;
    c.p.fillRect(footer, c.t.footerFill);
    c.mark("footer.fill", footer);
    const QRectF edge(footer.left(), footer.top(), footer.width(), std::max<qreal>(1, u * 0.15));
    c.p.fillRect(edge, c.t.footerTopEdge);
    c.mark("footer.edge", edge.adjusted(0, 0, 0, u * 0.5));
    const qreal centerX = footer.center().x();
    const QRectF name(centerX - footer.width() * 0.12, footer.top() + u * 1.4, footer.width() * 0.24, u * 1.2);
    c.bar(name, c.t.footerText);
    c.mark("footer.text", name);
    c.mark("app.text", name);

    const qreal trackY = footer.top() + footer.height() * 0.43;
    const qreal trackLeft = footer.left() + footer.width() * 0.16;
    const qreal trackWidth = footer.width() * 0.68;
    const qreal trackHeight = std::max<qreal>(1.5, u * 0.35);
    const QRectF track(trackLeft, trackY - trackHeight / 2, trackWidth, trackHeight);
    c.bar(track, c.t.progressGroove);
    c.mark("footer.track", track.adjusted(0, -u * 0.4, 0, u * 0.4));
    const qreal played = trackWidth * 0.42;
    const QRectF playedRect(trackLeft, track.top(), played, trackHeight);
    c.bar(playedRect, c.t.progressPlayed);
    c.mark("footer.played", playedRect.adjusted(0, -u * 0.4, 0, u * 0.4));
    c.mark("app.accent", playedRect.adjusted(0, -u * 0.4, 0, u * 0.4));
    const QRectF handle(trackLeft + played - u * 1.1, trackY - u * 1.1, u * 2.2, u * 2.2);
    c.dot(handle.center(), u * 1.1, c.t.progressHandle);
    c.mark("footer.handle", handle);
    c.mark("app.accent", handle);
    for (const qreal x : {trackLeft - u * 4.6, trackLeft + trackWidth + u * 1.2}) {
        const QRectF time(x, trackY - u * 0.4, u * 3.4, u * 0.8);
        c.bar(time, withAlpha(c.t.footerText, 0.8));
        c.mark("footer.text", time);
    }

    const QRectF switchTrack(centerX - u * 4.5, trackY + u * 1.6, u * 2.6, u * 1.4);
    c.bar(switchTrack, c.t.switchTrackOff);
    c.mark("footer.switch", switchTrack);
    const QRectF knob(switchTrack.left() + u * 0.2, switchTrack.top() + u * 0.2, u, u);
    c.dot(knob.center(), u * 0.5, c.t.switchKnob);
    c.mark("footer.knob", knob.adjusted(-u * 0.2, -u * 0.2, u * 0.2, u * 0.2));
    c.bar(QRectF(switchTrack.right() + u * 0.8, switchTrack.center().y() - u * 0.3, u * 6, u * 0.6),
          withAlpha(c.t.footerText, 0.8));

    const qreal buttonHeight = u * 2.6;
    const qreal buttonWidth = footer.width() * 0.13;
    const qreal buttonY = footer.bottom() - u * 1.4 - buttonHeight;
    const qreal gap = u * 1.2;
    for (int i = 0; i < 3; ++i) {
        const QRectF button(centerX - buttonWidth * 1.5 - gap + i * (buttonWidth + gap), buttonY, buttonWidth,
                            buttonHeight);
        c.box(button, buttonHeight / 2, c.t.transportFill);
        c.outline(button, buttonHeight / 2, i == 1 ? c.t.transportPlay : c.t.transportSkip,
                  std::max<qreal>(1, u * 0.3));
        c.mark(i == 1 ? "footer.play" : "footer.skip", button);
        c.mark("footer.buttonFill", button);
    }
    const QRectF loop(footer.left() + u * 2, buttonY + buttonHeight * 0.15, buttonHeight * 0.7, buttonHeight * 0.7);
    c.dot(loop.center(), loop.width() / 2, c.t.loopIcon);
    c.mark("footer.loop", loop);
    const QRectF speed(footer.right() - u * 2 - footer.width() * 0.07, buttonY, footer.width() * 0.07, buttonHeight);
    c.box(speed, u * 0.6, c.t.speedFieldBg);
    c.mark("footer.speedFill", speed);
    const QRectF speedText(speed.left() + speed.width() * 0.25, speed.center().y() - u * 0.35, speed.width() * 0.5,
                           u * 0.7);
    c.bar(speedText, c.t.speedFieldText);
    c.mark("footer.speedText", speedText.adjusted(-u * 0.3, -u * 0.3, u * 0.3, u * 0.3));
}

void paintDrawer(const Canvas &c, const QRectF &drawer)
{
    const qreal u = c.u;
    c.p.fillRect(drawer, c.t.drawerFill);
    c.mark("drawer.fill", drawer);
    const qreal left = drawer.left() + u * 1.4;
    const qreal width = drawer.width() - u * 2.8;
    qreal top = drawer.top() + u * 4;
    const qreal height = u * 2.6;
    const QRectF active(left, top, width, height);
    c.box(active, u * 0.8, c.t.navActiveStart);
    c.mark("drawer.activeStart", active);
    c.mark("app.accent", active);
    c.bar(QRectF(left + u * 1.2, active.center().y() - u * 0.4, width * 0.45, u * 0.8), c.t.accent.white);
    for (int i = 0; i < 3; ++i) {
        top += height + u * 0.9;
        const QRectF item(left + u * 1.2, top + height / 2 - u * 0.4, width * (0.5 - i * 0.06), u * 0.8);
        c.bar(item, c.t.navIdleText);
        c.mark("drawer.text", item);
    }
}

void paintMain(const Canvas &c, const QRectF &area)
{
    const qreal u = c.u;
    c.p.fillRect(area, c.t.windowMain);
    c.mark("main.window", area);
    const QRectF footer(area.left(), area.bottom() - area.height() * 0.3, area.width(), area.height() * 0.3);
    const QRectF drawer(area.left(), area.top(), area.width() * 0.2, footer.top() - area.top());
    const qreal contentLeft = drawer.right() + u * 3;
    const qreal contentRight = area.right() - u * 3;
    const qreal contentWidth = contentRight - contentLeft;

    const QRectF search(contentLeft + contentWidth * 0.2, area.top() + u * 1.6, contentWidth * 0.6, u * 3.2);
    c.box(search, u * 1.1, c.t.searchBg);
    c.mark("main.searchFill", search);
    const QRectF placeholder(search.left() + u * 1.4, search.center().y() - u * 0.4, search.width() * 0.35, u * 0.8);
    c.bar(placeholder, withAlpha(c.t.searchPlaceholder, 0.8));
    c.mark("main.searchText", placeholder);
    const QPointF lens(search.right() - u * 2, search.center().y());
    c.p.save();
    c.p.setPen(QPen(c.t.searchIcon, std::max<qreal>(1, u * 0.3)));
    c.p.setBrush(Qt::NoBrush);
    c.p.drawEllipse(lens, u * 0.8, u * 0.8);
    c.p.restore();
    c.mark("main.searchIcon", QRectF(lens.x() - u * 1.2, lens.y() - u * 1.2, u * 2.4, u * 2.4));

    const qreal headingTop = search.bottom() + u * 1.6;
    const QRectF heading(contentLeft, headingTop + u * 0.5, contentWidth * 0.22, u * 1.3);
    c.bar(heading, c.t.fontColor);
    c.mark("app.text", heading);
    const QRectF multi(contentRight - contentWidth * 0.18, headingTop, contentWidth * 0.18, u * 2.3);
    c.box(multi, u * 0.8, c.t.background);
    c.outline(multi, u * 0.8, c.t.neutralBorder, std::max<qreal>(1, u * 0.2));
    c.bar(QRectF(multi.left() + multi.width() * 0.2, multi.center().y() - u * 0.3, multi.width() * 0.6, u * 0.6),
          c.t.fontColor);
    c.mark("main.buttonBorder", multi);
    c.mark("app.surface", multi);

    const qreal cardTop = heading.bottom() + u * 1.4;
    const qreal cardHeight = (footer.top() - cardTop - u * 2.6) / 2;
    for (int i = 0; i < 2; ++i) {
        paintCard(c, QRectF(contentLeft, cardTop + i * (cardHeight + u * 1.2), contentWidth - u * 1.6, cardHeight), i == 0);
    }
    const QRectF scroll(contentRight - u * 0.5, cardTop, u * 0.5, footer.top() - cardTop - u * 1.4);
    c.bar(QRectF(scroll.left(), scroll.top(), scroll.width(), scroll.height() * 0.45), c.t.scrollHandle);
    c.mark("main.scrollbar", scroll.adjusted(-u * 0.3, 0, u * 0.3, 0));
    paintDrawer(c, drawer);
    paintFooter(c, footer);
}

void paintPopups(const Canvas &c, const QRectF &area)
{
    const qreal u = c.u;
    paintMain(c, area);
    c.p.fillRect(area, c.t.overlayDim);
    c.mark("popup.dim", area);

    const QRectF menu(area.right() - area.width() * 0.34, area.top() + area.height() * 0.2, area.width() * 0.26,
                      u * 13);
    c.box(menu, u * 1.2, c.t.popoverFill);
    c.outline(menu, u * 1.2, c.t.popoverBorder, std::max<qreal>(1, u * 0.2));
    c.mark("popup.menuFill", menu);
    c.mark("popup.menuBorder", menu);
    for (int i = 0; i < 4; ++i) {
        const QRectF row(menu.left() + u * 0.8, menu.top() + u * 0.8 + i * u * 2.9, menu.width() - u * 1.6, u * 2.6);
        QColor ink = c.t.fontColor;
        if (i == 1) {
            c.box(row, u * 0.7, c.t.menuHoverFill);
            ink = c.t.menuHoverText;
            c.mark("popup.menuHover", row);
        } else if (i == 3) {
            ink = c.t.dangerText;
        }
        c.bar(QRectF(row.left() + u * 1.2, row.center().y() - u * 0.35, row.width() * (0.55 - i * 0.05), u * 0.7), ink);
    }

    const QRectF dialog(area.left() + area.width() * 0.2, area.top() + area.height() * 0.48, area.width() * 0.46,
                        area.height() * 0.4);
    c.box(dialog, u * 1.4, c.t.confirmFill);
    c.mark("popup.dialogFill", dialog);
    const QRectF message(dialog.left() + u * 1.4, dialog.top() + u * 1.4, dialog.width() - u * 2.8,
                         dialog.height() * 0.48);
    c.box(message, u * 0.8, c.t.confirmMessageFill);
    for (int i = 0; i < 2; ++i) {
        const QRectF line(message.left() + u * 1.2, message.top() + u * 1.3 + i * u * 1.8, message.width() * (0.8 - i * 0.25),
                          u * 0.8);
        c.bar(line, c.t.confirmText);
        c.mark("popup.dialogText", line);
    }
    const qreal buttonTop = dialog.bottom() - u * 3.6;
    const QRectF ok(dialog.center().x() + u * 0.6, buttonTop, dialog.width() * 0.3, u * 2.4);
    c.box(ok, u * 0.8, c.t.confirmOk);
    c.bar(QRectF(ok.left() + ok.width() * 0.3, ok.center().y() - u * 0.3, ok.width() * 0.4, u * 0.6), c.t.accent.white);
    c.mark("popup.confirm", ok);
    const QRectF cancel(dialog.center().x() - u * 0.6 - dialog.width() * 0.3, buttonTop, dialog.width() * 0.3, u * 2.4);
    c.box(cancel, u * 0.8, c.t.confirmCancel);
    c.bar(QRectF(cancel.left() + cancel.width() * 0.3, cancel.center().y() - u * 0.3, cancel.width() * 0.4, u * 0.6),
          c.t.accent.white);
}

void paintAlerts(const Canvas &c, const QRectF &area)
{
    const qreal u = c.u;
    paintMain(c, area);
    const QColor bases[] = {c.t.toastSuccess, c.t.toastWarning, c.t.toastError, c.t.toastInfo};
    const char *ids[] = {"toast.success", "toast.warning", "toast.error", "toast.info"};
    const qreal width = area.width() * 0.42;
    const qreal height = u * 3.6;
    for (int i = 0; i < 4; ++i) {
        const QRectF toast(area.right() - width - u * 2, area.top() + u * 1.6 + i * (height + u * 0.9), width, height);
        c.box(toast, u * 1.0, c.t.background);
        c.box(toast, u * 1.0, withAlpha(bases[i], 0.18));
        c.outline(toast, u * 1.0, withAlpha(bases[i], 0.35), std::max<qreal>(1, u * 0.2));
        c.dot(QPointF(toast.left() + u * 2, toast.center().y()), u * 0.8, bases[i]);
        c.mark(ids[i], toast);
        const QRectF text(toast.left() + u * 3.6, toast.center().y() - u * 0.4, width * (0.55 - i * 0.05), u * 0.8);
        c.bar(text, c.t.toastText);
        c.mark("toast.text", text);
    }
}

void paintSettings(const Canvas &c, const QRectF &area)
{
    const qreal u = c.u;
    const SettingsPalette &s = c.t.settings;
    const QRectF sidebar(area.left(), area.top(), area.width() * 0.27, area.height());
    const QRectF content(sidebar.right(), area.top(), area.right() - sidebar.right(), area.height());
    c.p.fillRect(content, s.contentFill);
    c.mark("settings.content", content);
    c.p.fillRect(sidebar, s.sidebarFill);
    c.mark("settings.sidebar", sidebar);
    const QRectF edge(sidebar.right() - std::max<qreal>(1, u * 0.15), sidebar.top(), std::max<qreal>(1, u * 0.15),
                      sidebar.height());
    c.p.fillRect(edge, s.sidebarEdge);
    c.mark("settings.sidebarEdge", edge.adjusted(-u * 0.4, 0, u * 0.4, 0));

    c.box(QRectF(sidebar.left() + u * 1.4, sidebar.top() + u * 1.6, u * 2.4, u * 2.4), u * 0.6, c.t.accent.blue600);
    c.bar(QRectF(sidebar.left() + u * 4.4, sidebar.top() + u * 2.3, sidebar.width() * 0.4, u), c.t.fontColor);
    for (int i = 0; i < 5; ++i) {
        const QRectF item(sidebar.left() + u * 1.2, sidebar.top() + u * 6 + i * u * 3, sidebar.width() - u * 2.4, u * 2.4);
        if (i == 3) {
            c.box(item, u * 0.7, s.subtleHoverFill);
        }
        c.bar(QRectF(item.left() + u * 1.2, item.center().y() - u * 0.35, item.width() * (0.55 - (i % 3) * 0.08), u * 0.7),
              c.t.fontColor);
    }
    c.box(QRectF(sidebar.left() + u * 1.2, sidebar.bottom() - u * 5, sidebar.width() - u * 2.4, u * 3.6), u * 0.8,
          s.subtleFill);

    const qreal left = content.left() + u * 2.4;
    const qreal width = content.width() - u * 4.8;
    const QRectF title(left, content.top() + u * 1.8, width * 0.3, u * 1.4);
    c.bar(title, c.t.fontColor);
    const QRectF description(left, title.bottom() + u * 0.8, width * 0.55, u * 0.8);
    c.bar(description, s.headerDescText);
    c.mark("settings.description", description);
    c.p.fillRect(QRectF(left, description.bottom() + u * 1.2, width, std::max<qreal>(1, u * 0.12)), s.headerRule);

    qreal top = description.bottom() + u * 2.4;
    const qreal cardHeight = u * 5;
    for (int i = 0; i < 3; ++i) {
        const QRectF card(left, top, width, cardHeight);
        c.box(card, u * 1.2, s.cardFill);
        c.outline(card, u * 1.2, s.cardBorder, std::max<qreal>(1, u * 0.18));
        c.mark("settings.card", card);
        c.mark("settings.cardBorder", card);
        c.dot(QPointF(card.left() + u * 2.2, card.center().y()), u * 0.9, withAlpha(c.t.fontColor, 0.7));
        c.bar(QRectF(card.left() + u * 4, card.top() + u * 1.3, card.width() * 0.3, u * 0.9), c.t.fontColor);
        const QRectF cardDesc(card.left() + u * 4, card.top() + u * 2.9, card.width() * 0.45, u * 0.7);
        c.bar(cardDesc, s.cardDescText);
        c.mark("settings.description", cardDesc);
        if (i == 0) {
            const QRectF toggle(card.right() - u * 5.4, card.center().y() - u * 0.9, u * 3.6, u * 1.8);
            c.bar(toggle, s.checkStart);
            c.dot(QPointF(toggle.right() - u * 0.9, toggle.center().y()), u * 0.65, s.knob);
            c.mark("settings.switch", toggle);
        } else {
            const QRectF control(card.right() - u * 8, card.center().y() - u * 1.1, u * 6.6, u * 2.2);
            c.box(control, u * 0.6, s.subtleFill);
            c.outline(control, u * 0.6, s.subtleBorder, std::max<qreal>(1, u * 0.15));
        }
        top += cardHeight + u * 1.2;
    }
    const QRectF save(content.right() - u * 5.6, content.bottom() - u * 5.6, u * 4, u * 4);
    c.dot(save.center(), save.width() / 2, s.saveStart);
    c.mark("settings.save", save);
}

void paintEditor(const Canvas &c, const QRectF &area)
{
    const qreal u = c.u;
    c.p.fillRect(area, c.t.windowEditor);
    c.mark("editor.window", area);

    constexpr int columns = 4;
    const qreal side = area.width() * 0.03;
    const qreal gap = area.width() * 0.023;
    const qreal gridRight = area.right() - side - u * 1.4;
    const qreal cellWidth = (gridRight - area.left() - side - gap * (columns - 1)) / columns;
    const qreal cellHeight = cellWidth * 0.55;
    const qreal rowStep = cellHeight + u * 3.2;
    const qreal gridTop = area.top() + u * 1.6;
    const qreal gridBottom = area.top() + area.height() * 0.58;
    const int notes[][3] = {{4, -1, -1}, {8, -1, -1}, {8, -1, -1}, {0, 3, 8},
                            {9, -1, -1}, {9, -1, -1}, {9, -1, -1}, {9, -1, -1},
                            {-1, -1, -1}, {1, 4, -1}, {-1, -1, -1}, {-1, -1, -1}};
    c.p.save();
    c.p.setClipRect(QRectF(area.left(), area.top(), area.width(), gridBottom - area.top()));
    for (int cell = 0; cell < 12; ++cell) {
        const int row = cell / columns;
        const int column = cell % columns;
        const QRectF rect(area.left() + side + column * (cellWidth + gap), gridTop + row * rowStep, cellWidth, cellHeight);
        QColor fill = c.t.gridColor;
        const char *cellId = "editor.grid";
        if (cell == 5) {
            fill = c.t.activeColor;
            cellId = "editor.active";
        } else if (cell == 2) {
            fill = c.t.gridHoverColor;
            cellId = "editor.hover";
        }
        c.box(rect, u * 0.7, fill);
        if (rect.bottom() < gridBottom) {
            c.mark(cellId, rect);
        }
        const qreal stepX = rect.width() / 6;
        const qreal stepY = rect.height() / 4;
        for (int dot = 0; dot < 15; ++dot) {
            const QPointF center(rect.left() + stepX * (1 + dot % 5), rect.top() + stepY * (1 + dot / 5));
            const int *list = notes[cell];
            const bool note = list[0] == dot || list[1] == dot || list[2] == dot;
            if (note) {
                const QRectF square(center.x() - u * 0.6, center.y() - u * 0.6, u * 1.2, u * 1.2);
                c.box(square, u * 0.2, c.t.gridNoteColor);
                if (rect.bottom() < gridBottom) {
                    c.mark("editor.note", square.adjusted(-u * 0.3, -u * 0.3, u * 0.3, u * 0.3));
                }
            } else {
                c.dot(center, std::max<qreal>(0.6, u * 0.16), c.t.dotColor);
                if (dot == 7 && cell == 0) {
                    c.mark("editor.dot", QRectF(center.x() - u * 0.6, center.y() - u * 0.6, u * 1.2, u * 1.2));
                }
            }
        }
        c.bar(QRectF(rect.center().x() - rect.width() * 0.12, rect.bottom() + u * 1.0, rect.width() * 0.24, u * 0.6),
              withAlpha(c.t.fontColor, 0.65));
    }
    c.p.restore();
    const QRectF scroll(gridRight + u * 0.6, gridTop, u * 0.5, gridBottom - gridTop - u);
    c.bar(QRectF(scroll.left(), scroll.top(), scroll.width(), u * 2.4), c.t.scrollHandle);

    const qreal keyGap = u * 0.9;
    const qreal keySize = (area.bottom() - gridBottom - u * 4.2 - keyGap * 2) / 3;
    const qreal padWidth = keySize * 5 + keyGap * 4;
    const qreal padLeft = area.left() + area.width() * 0.47 - padWidth / 2;
    const qreal padTop = gridBottom + u * 2.2;
    const qreal stroke = std::max<qreal>(0.8, u * 0.18);
    for (int key = 0; key < 15; ++key) {
        const QRectF rect(padLeft + (key % 5) * (keySize + keyGap), padTop + (key / 5) * (keySize + keyGap), keySize,
                          keySize);
        const bool pressed = key == 7;
        c.box(rect, u * 0.5, pressed ? c.t.activeColor : c.t.gridColor);
        c.mark(pressed ? "editor.active" : "editor.grid", rect);
        const QPointF center = rect.center();
        const qreal reach = keySize * 0.32;
        c.p.save();
        c.p.setPen(QPen(c.t.fontColor, stroke));
        c.p.setBrush(Qt::NoBrush);
        if (key % 2 == 0) {
            c.p.drawEllipse(center, reach, reach);
        }
        if (key % 2 == 1 || key == 0 || key == 7 || key == 14) {
            QPolygonF diamond;
            diamond << QPointF(center.x(), center.y() - reach * 1.15) << QPointF(center.x() + reach * 1.15, center.y())
                    << QPointF(center.x(), center.y() + reach * 1.15) << QPointF(center.x() - reach * 1.15, center.y());
            c.p.drawPolygon(diamond);
        }
        c.p.restore();
        c.dot(center, std::max<qreal>(0.6, u * 0.2), c.t.fontColor);
    }

    const QRectF panel(area.left() + side, area.bottom() - area.height() * 0.32, area.width() * 0.19,
                       area.height() * 0.28);
    c.box(panel, u * 1.0, c.t.editor.metaFill);
    c.outline(panel, u * 1.0, c.t.editor.metaBorder, std::max<qreal>(1, u * 0.15));
    c.mark("editor.panel", panel);
    for (int i = 0; i < 4; ++i) {
        c.bar(QRectF(panel.left() + u * 1.4, panel.top() + u * 1.5 + i * panel.height() * 0.22,
                     panel.width() * (0.5 + (i % 2) * 0.15), u * 0.6),
              withAlpha(c.t.fontColor, 0.8));
    }

    const QRectF button(area.right() - area.width() * 0.19, area.bottom() - area.height() * 0.17, area.width() * 0.08,
                        u * 2.6);
    c.box(button, u * 0.5, c.t.editor.exportFill);
    c.bar(QRectF(button.left() + button.width() * 0.22, button.center().y() - u * 0.3, button.width() * 0.56, u * 0.6),
          c.t.editor.exportText);
    c.mark("editor.export", button);
}

void paintScene(QPainter &painter, const QRectF &rect, const Theme &theme, qreal radius, PreviewScene scene,
                Regions *regions)
{
    painter.save();
    painter.setClipPath(roundedPath(rect, radius), painter.hasClipping() ? Qt::IntersectClip : Qt::ReplaceClip);
    const Canvas canvas(painter, theme, rect.height() / 50.0, regions);
    const QRectF area = titleStrip(canvas, rect);
    switch (scene) {
    case PreviewScene::Main:
        paintMain(canvas, area);
        break;
    case PreviewScene::Popups:
        paintPopups(canvas, area);
        break;
    case PreviewScene::Alerts:
        paintAlerts(canvas, area);
        break;
    case PreviewScene::Settings:
        paintSettings(canvas, area);
        break;
    case PreviewScene::Editor:
        paintEditor(canvas, area);
        break;
    }
    canvas.outline(rect, radius, theme.windowBorder, std::max<qreal>(1, canvas.u * 0.18));
    canvas.mark("title.border", rect);
    painter.restore();
}

QImage renderScene(const QSizeF &size, const Theme &theme, qreal radius, qreal dpr, PreviewScene scene,
                   Regions *regions)
{
    QImage image((size * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    preparePainter(painter);
    paintScene(painter, QRectF(QPointF(0, 0), size), theme, radius, scene, regions);
    return image;
}

void paintModePill(QPainter &painter, const QPointF &anchor, bool alignRight, AppTheme kind, bool withText,
                   bool highlighted, const QColor &accent, qreal dpr)
{
    const QString label = kind == AppTheme::Dark ? QStringLiteral("Dark") : QStringLiteral("Light");
    const QString iconId = kind == AppTheme::Dark ? QStringLiteral("moon") : QStringLiteral("sun");
    const QFont font = FontLoader::font(11, QFont::DemiBold);
    const qreal icon = withText ? 12 : 11;
    const qreal height = withText ? 22 : 20;
    const qreal textWidth = withText ? QFontMetricsF(font).horizontalAdvance(label) + 5 : 0;
    const qreal width = withText ? icon + textWidth + 18 : height;
    const QRectF pill(alignRight ? anchor.x() - width : anchor.x(), anchor.y(), width, height);
    paintPill(painter, pill, highlighted ? accent : QColor(0, 0, 0, 150));
    const qreal iconLeft = withText ? pill.left() + 8 : pill.center().x() - icon / 2;
    painter.drawPixmap(QPointF(iconLeft, pill.center().y() - icon / 2),
                       IconCache::pixmap(iconId, static_cast<int>(icon), Qt::white, dpr));
    if (withText) {
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(iconLeft + icon + 5, pill.top(), textWidth, pill.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, label);
    }
}

int paintFocus(QPainter &painter, const QRectF &rect, qreal radius, const Regions &regions, const QStringList &focus)
{
    if (focus.isEmpty()) {
        return 0;
    }
    QPainterPath spots;
    spots.setFillRule(Qt::WindingFill);
    int matched = 0;
    for (const auto &[id, area] : regions) {
        if (focus.contains(id)) {
            const QRectF shown = area.translated(rect.topLeft()).adjusted(-1.5, -1.5, 1.5, 1.5).intersected(rect);
            spots.addRoundedRect(shown, 3, 3);
            ++matched;
        }
    }
    if (matched == 0) {
        return 0;
    }
    spots = spots.simplified();
    painter.save();
    painter.setClipPath(roundedPath(rect, radius));
    QPainterPath shade = roundedPath(rect, radius).subtracted(spots);
    painter.fillPath(shade, QColor(0, 0, 0, 120));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 120), 3));
    painter.drawPath(spots);
    painter.setPen(QPen(QColor(255, 255, 255, 240), 1.5));
    painter.drawPath(spots);
    painter.restore();
    return matched;
}

}  // namespace

QStringList previewSlotIds(PreviewScene scene)
{
    QImage image(500, 280, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    Regions regions;
    paintScene(painter, QRectF(0, 0, 500, 280), Theme::light(), 12, scene, &regions);
    QStringList ids;
    for (const auto &region : regions) {
        if (!ids.contains(region.first)) {
            ids << region.first;
        }
    }
    return ids;
}

void paintThemePreview(QPainter &painter, const QRectF &rect, const Theme &theme, qreal radius, PreviewScene scene)
{
    paintScene(painter, rect, theme, radius, scene, nullptr);
}

int paintThemeSplitPreview(QPainter &painter, const QRectF &rect, const Theme &light, const Theme &dark, qreal radius,
                           bool withText, std::optional<AppTheme> highlight, PreviewScene scene,
                           const QStringList &focus, bool badges)
{
    painter.save();
    const qreal dpr = painter.device()->devicePixelRatioF();
    Regions regions;
    painter.drawImage(rect.topLeft(), renderScene(rect.size(), light, radius, dpr, scene, &regions));
    const QPointF top(rect.left() + rect.width() * kSplitTop, rect.top());
    const QPointF bottom(rect.left() + rect.width() * kSplitBottom, rect.bottom());
    QPainterPath darkSide;
    darkSide.moveTo(top);
    darkSide.lineTo(rect.topRight() + QPointF(1, 0));
    darkSide.lineTo(rect.bottomRight() + QPointF(1, 1));
    darkSide.lineTo(bottom + QPointF(0, 1));
    darkSide.closeSubpath();
    painter.setClipPath(darkSide);
    painter.drawImage(rect.topLeft(), renderScene(rect.size(), dark, radius, dpr, scene, nullptr));
    painter.setClipping(false);

    painter.setClipPath(roundedPath(rect, radius));
    painter.setPen(QPen(QColor(0, 0, 0, 70), 4));
    painter.drawLine(top, bottom);
    painter.setPen(QPen(QColor(255, 255, 255, 230), 2));
    painter.drawLine(top, bottom);
    painter.setClipping(false);

    const int matched = paintFocus(painter, rect, radius, regions, focus);

    if (badges) {
        const qreal inset = withText ? 10 : 7;
        paintModePill(painter, QPointF(rect.left() + inset, rect.bottom() - inset - (withText ? 22 : 20)), false,
                      AppTheme::Light, withText, highlight == AppTheme::Light, light.hoverColor, dpr);
        paintModePill(painter, QPointF(rect.right() - inset, rect.top() + inset + (withText ? 18 : 12)), true,
                      AppTheme::Dark, withText, highlight == AppTheme::Dark, dark.hoverColor, dpr);
    }

    painter.setPen(QPen(withAlpha(currentTheme().fontColor, 0.16), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius));
    painter.restore();
    return matched;
}

ThemePreview::ThemePreview(QWidget *parent) : QWidget(parent), m_light(Theme::light()), m_dark(Theme::dark())
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        m_cache = QPixmap();
        update();
    });
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}

void ThemePreview::setThemes(const Theme &light, const Theme &dark, std::optional<AppTheme> highlight)
{
    m_light = light;
    m_dark = dark;
    m_highlight = highlight;
    m_cache = QPixmap();
    update();
}

void ThemePreview::setModeBadges(bool visible)
{
    m_badges = visible;
    m_cache = QPixmap();
    update();
}

void ThemePreview::showFocus(PreviewScene scene, const QStringList &slotIds)
{
    if (scene == m_scene && slotIds == m_focus) {
        return;
    }
    m_scene = scene;
    m_focus = slotIds;
    m_cache = QPixmap();
    update();
}

int ThemePreview::heightForWidth(int width) const
{
    return std::min(kMaxHeight, qRound(width * kAspect));
}

QSize ThemePreview::sizeHint() const
{
    return {480, heightForWidth(480)};
}

void ThemePreview::paintEvent(QPaintEvent *)
{
    const qreal height = std::min<qreal>(this->height(), kMaxHeight);
    const qreal width = std::min<qreal>(this->width(), height / kAspect);
    const QRectF area((this->width() - width) / 2, (this->height() - height) / 2, width, height);
    const qreal dpr = devicePixelRatioF();
    const QSize cacheSize = (area.size() * dpr).toSize();
    if (m_cache.size() != cacheSize || !qFuzzyCompare(m_cache.devicePixelRatio(), dpr)) {
        m_cache = QPixmap(cacheSize);
        m_cache.setDevicePixelRatio(dpr);
        m_cache.fill(Qt::transparent);
        QPainter cache(&m_cache);
        preparePainter(cache);
        paintThemeSplitPreview(cache, QRectF(QPointF(0, 0), area.size()), m_light, m_dark, 14, true, m_highlight,
                               m_scene, m_focus, m_badges);
    }
    QPainter painter(this);
    painter.drawPixmap(area.topLeft(), m_cache);
}

}  // namespace sky
