#pragma once

#include <optional>

#include <QColor>
#include <QMap>
#include <QPointF>
#include <QString>

#include "model/config.hpp"

namespace sky {

struct ShadowSpec {
    qreal blur = 0;
    QPointF offset;
    QColor color;
};

struct AccentPalette {
    QColor blue600, blue700, blue500, blue400;
    QColor sky500, sky600, sky400;
    QColor green500, green600, green700;
    QColor emerald500, emerald400;
    QColor amber500, amber600, amber400;
    QColor warnStart, warnEnd;
    QColor violet400, purple400;
    QColor red500, red600, red700, red800;
    QColor rose500, rose400;
    QColor slate700, slate600, slate500, slate400, slate300, slate200;
    QColor gray200, gray100, gray50;
    QColor ink900, darkSurface;
    QColor white, black, transparent;
    QColor gradientEnd;
};

struct SettingsPalette {
    QColor windowFill, contentFill, sidebarFill, sidebarEdge, headerRule;
    QColor navIconSliders, navIconKeyboard, navIconLightning, navIconRefresh;
    QColor headerDescText, cardDescText;
    QColor importFill, importBorder, importText, importHoverFill;
    QColor cardFill, cardBorder, cardHoverBorder, tileStart, tileEnd, tileGlyph;
    QColor warnBorder, warnHoverBorder, warnFillStart, warnFillEnd, warnText, warnTileStart, warnTileEnd;
    QColor trackFill, trackBorder, trackOnBorder, knob;
    QColor selectBorder, selectListFill, selectListText;
    QColor keyCapFill, keyCapBorder, keyCapHoverFill, keyGlyphFocusFill, keyShape, tipText;
    QColor shortcutFill, shortcutBorder, shortcutText, shortcutFocusFill, shortcutFocusBorder, shortcutRing;
    QColor checkStart, checkEnd, saveStart, saveEnd;
    QColor statusSuccessFill, statusSuccessText, statusSuccessBorder;
    QColor statusErrorFill, statusErrorText, statusErrorBorder;
    QColor modalDim, modalFill, modalBorder;
    QColor cancelFill, cancelBorder, cancelText;
    QColor subtleFill, subtleHoverFill, subtleBorder, discordFill, amberSoft, amberSolid, amberSolidHover;
    ShadowSpec sidebarShadow, cardHoverShadow, tileShadow, warnHoverShadow, warnTileShadow, knobShadow, modalShadow;
};

struct EditorPalette {
    QColor metaFill, metaBorder, rowHoverFill, rowEditFill, valueHoverFill, holdBannerFill;
    QColor exportFill, exportHoverFill, exportText, actionText, glowRing;
    ShadowSpec metaShadow, glowDrop;
    bool noteGlow = false;
};

struct UpdatePalette {
    QColor changelogDim, changelogBorder, rule, pillStart, pillEnd, pillBorder, codeFill;
    QColor promptFill, promptBorder, promptHeading, promptVersion, promptBody, laterBorder, laterText;
    QColor versionOld, versionNew, progressTrack, statusText, statusError;
    ShadowSpec changelogShadow, pillShadow, gotItShadow;
};

struct CoreColors {
    std::optional<QColor> background;
    std::optional<QColor> fontColor;
    std::optional<QColor> hoverColor;
};

struct Theme {
    AppTheme kind = AppTheme::Light;

    QColor background, surfaceDeep, fontColor, hoverColor, borderColor, successColor, errorColor;
    QColor gridColor, dotColor, gridNoteColor, gridHoverColor, activeColor, editBg;

    QColor windowMain, windowSettings, windowEditor, prePaint;
    QColor titleBarFill, titleBarText, windowBorder;

    QColor searchBg, searchText, searchPlaceholder, searchIcon;
    QColor cardFill, cardText;
    QColor cardTopEdge;
    int cardTopEdgeHeight = 1;
    QColor footerFill, footerText, footerTopEdge;
    QColor holdBadgeFill, cardLabelTint;
    QColor authorIcon, transcriberIcon, bpmIcon, durationIcon;
    QColor cardSelectedOutline, cardSelectedTint;
    QColor actionGlyph, actionHoverFill, actionHoverGlyph, actionOpenFill, actionOpenGlyph;
    QColor popoverFill, popoverBorder, menuHoverFill, menuHoverText, dangerText, dangerHoverFill, dangerHoverText;
    QColor heartOn, heartOff, editIcon, trashIcon;
    QColor drawerFill, navIdleText, navHoverText, navHoverFill, navActiveStart, navActiveEnd, navSweep;
    QColor bulkBorder, bulkFill, bulkCount;
    QColor overlayDim, pickerBorder, pickerFieldBorder, pickerFieldFocus, pickerFieldRing, pickerRowRule;
    QColor confirmOverlay, confirmFill, confirmMessageFill, confirmText, confirmRule, confirmButtonHover;
    QColor confirmOk, confirmCancel;
    QColor storeBadgeFill, storeBadgeText, storePlay, storePlayHover, storePlayPressed, storePlayHoverFill,
        storePlayPressedFill, storePlayDisabledBorder, storePlayDisabledText;

    QColor neutralBorder, neutralBorderDisabled, neutralFillDisabled;
    QColor transportFill, transportSkip, transportSkipHover, transportPlay, transportPlayHover;
    QColor progressGroove, progressPlayed, progressHandle;
    QColor speedFieldBg, speedFieldText;
    QColor switchTrackOff, switchKnob, switchKnobShadow;
    QColor loopIcon, loopIconHover;
    QColor sunIcon, moonIcon;
    QColor scrollHandle, scrollHandleHover, scrollGroove;

    QColor toastText, toastTextShadow, toastBorderHighlight, toastProgress, toastClose;
    QColor toastSuccess, toastWarning, toastError, toastInfo, toastNeutral;

    ShadowSpec shadowCard, shadowFooter, shadowControl, shadowToast, shadowPopover;
    ShadowSpec shadowDrawer, shadowBulk, shadowPicker, shadowConfirm, shadowPageActive,
        shadowDestructive;

    AccentPalette accent;
    SettingsPalette settings;
    EditorPalette editor;
    UpdatePalette update;

    QMap<QString, QString> qssTokens() const;
    QColor toastBase(int type) const;

    static Theme build(AppTheme kind, const CoreColors &core = {});
    static const Theme &light();
    static const Theme &dark();
    static const Theme &forKind(AppTheme kind);
};

QString cssColor(const QColor &color);
QColor withAlpha(QColor color, qreal alpha);
QColor blend(const QColor &from, const QColor &to, qreal amount);

}  // namespace sky
