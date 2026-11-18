#include "ui/theme.hpp"

namespace sky {

namespace {

QColor rgba(int r, int g, int b, qreal a)
{
    return QColor(r, g, b, qRound(a * 255));
}

AccentPalette accents()
{
    AccentPalette p;
    p.blue600 = QColor("#2563eb");
    p.blue700 = QColor("#1d4ed8");
    p.blue500 = QColor("#3b82f6");
    p.blue400 = QColor("#60a5fa");
    p.sky500 = QColor("#0ea5e9");
    p.sky600 = QColor("#0284c7");
    p.sky400 = QColor("#38bdf8");
    p.green500 = QColor("#22c55e");
    p.green600 = QColor("#16a34a");
    p.green700 = QColor("#15803d");
    p.emerald500 = QColor("#10b981");
    p.emerald400 = QColor("#34d399");
    p.amber500 = QColor("#f59e0b");
    p.amber600 = QColor("#d97706");
    p.amber400 = QColor("#fbbf24");
    p.warnStart = QColor("#ffc107");
    p.warnEnd = QColor("#ff9800");
    p.violet400 = QColor("#a78bfa");
    p.purple400 = QColor("#c084fc");
    p.red500 = QColor("#ef4444");
    p.red600 = QColor("#dc2626");
    p.red700 = QColor("#b91c1c");
    p.red800 = QColor("#991b1b");
    p.rose500 = QColor("#f43f5e");
    p.rose400 = QColor("#fb7185");
    p.slate700 = QColor("#334155");
    p.slate600 = QColor("#475569");
    p.slate500 = QColor("#64748b");
    p.slate400 = QColor("#94a3b8");
    p.slate300 = QColor("#cbd5e1");
    p.slate200 = QColor("#e2e8f0");
    p.gray200 = QColor("#e5e7eb");
    p.gray100 = QColor("#f3f4f6");
    p.gray50 = QColor("#f9fafb");
    p.ink900 = QColor("#0f172a");
    p.darkSurface = QColor("#1a1a1a");
    p.white = QColor("#ffffff");
    p.black = QColor("#000000");
    p.transparent = rgba(0, 0, 0, 0);
    p.gradientEnd = QColor("#4a90e2");
    return p;
}

void applyShared(Theme &t)
{
    t.accent = accents();
    const AccentPalette &a = t.accent;
    t.holdBadgeFill = rgba(37, 99, 235, 0.14);
    t.durationIcon = a.purple400;
    t.cardSelectedOutline = a.blue500;
    t.cardSelectedTint = a.blue600;
    t.actionHoverFill = rgba(59, 130, 246, 0.14);
    t.actionHoverGlyph = a.blue500;
    t.actionOpenFill = a.slate700;
    t.actionOpenGlyph = a.white;
    t.neutralBorderDisabled = rgba(148, 163, 184, 0.28);
    t.neutralFillDisabled = rgba(148, 163, 184, 0.1);
    t.transportPlay = a.green500;
    t.transportPlayHover = a.green600;
    t.switchKnobShadow = rgba(37, 34, 71, 0.1);
    t.sunIcon = a.amber500;
    t.moonIcon = a.violet400;
    t.toastBorderHighlight = rgba(255, 255, 255, 0.1);
    t.toastProgress = rgba(255, 255, 255, 0.5);
    t.toastClose = a.white;
    t.toastSuccess = QColor(46, 204, 113);
    t.toastWarning = QColor(238, 184, 73);
    t.toastError = QColor(231, 76, 60);
    t.toastInfo = QColor(52, 152, 219);
    t.toastNeutral = QColor(160, 160, 160);
    t.menuHoverFill = rgba(59, 130, 246, 0.12);
    t.menuHoverText = a.blue500;
    t.dangerText = a.red500;
    t.dangerHoverFill = rgba(239, 68, 68, 0.12);
    t.dangerHoverText = a.red600;
    t.editIcon = a.emerald500;
    t.trashIcon = a.red500;
    t.navSweep = rgba(255, 255, 255, 0.1);
    t.bulkBorder = rgba(148, 163, 184, 0.25);
    t.bulkCount = a.blue400;
    t.overlayDim = rgba(0, 0, 0, 0.55);
    t.pickerFieldBorder = rgba(148, 163, 184, 0.45);
    t.pickerFieldFocus = a.blue500;
    t.pickerFieldRing = rgba(59, 130, 246, 0.18);
    t.pickerRowRule = rgba(148, 163, 184, 0.2);
    t.confirmOverlay = rgba(10, 15, 20, 0.25);
    t.confirmFill = rgba(255, 255, 255, 0.95);
    t.confirmMessageFill = rgba(247, 247, 247, 0.8);
    t.confirmText = QColor("#333333");
    t.confirmRule = rgba(238, 238, 238, 0.5);
    t.confirmButtonHover = rgba(245, 245, 245, 0.8);
    t.confirmOk = QColor("#2196F3");
    t.confirmCancel = QColor("#607D8B");
    t.storePlay = a.green500;
    t.storePlayHover = a.green600;
    t.storePlayPressed = a.green700;
    t.storePlayHoverFill = rgba(34, 197, 94, 0.1);
    t.storePlayPressedFill = rgba(34, 197, 94, 0.18);
    t.storePlayDisabledBorder = rgba(34, 197, 94, 0.45);
    t.storePlayDisabledText = rgba(34, 197, 94, 0.55);
    t.shadowDrawer = {22, {8, 0}, rgba(0, 0, 0, 0.22)};
    t.shadowBulk = {30, {0, 12}, rgba(0, 0, 0, 0.2)};
    t.shadowPicker = {42, {0, 18}, rgba(0, 0, 0, 0.45)};
    t.shadowConfirm = {24, {0, 8}, rgba(0, 0, 0, 0.2)};
    t.shadowPageActive = {12, {0, 3}, rgba(37, 99, 235, 0.24)};
    t.shadowDestructive = {12, {0, 3}, rgba(185, 28, 28, 0.22)};
    t.shadowCard = {16, {0, 0}, rgba(0, 0, 0, 0.2)};
    t.shadowControl = {12, {0, 3}, rgba(0, 0, 0, 0.18)};
    t.shadowToast = {30, {0, 12}, rgba(0, 0, 0, 0.25)};
}

void applySharedSettings(Theme &t)
{
    const AccentPalette &a = t.accent;
    SettingsPalette &s = t.settings;
    s.discordFill = QColor(88, 101, 242);
    s.amberSolid = a.amber600;
    s.amberSolidHover = a.amber500;
    s.tileEnd = a.gradientEnd;
    s.tileGlyph = a.white;
    s.warnTileStart = a.warnStart;
    s.warnTileEnd = a.warnEnd;
    s.knob = a.white;
    s.shortcutFocusBorder = a.emerald500;
    s.shortcutRing = rgba(16, 185, 129, 0.2);
    s.saveStart = QColor("#2ecc71");
    s.saveEnd = QColor("#27ae60");
    s.statusSuccessFill = rgba(46, 204, 113, 0.2);
    s.statusSuccessText = QColor("#2ecc71");
    s.statusSuccessBorder = rgba(46, 204, 113, 0.3);
    s.statusErrorFill = rgba(231, 76, 60, 0.2);
    s.statusErrorText = QColor("#e74c3c");
    s.statusErrorBorder = rgba(231, 76, 60, 0.3);
    s.modalDim = rgba(0, 0, 0, 0.7);
    s.sidebarShadow = {10, {2, 0}, rgba(0, 0, 0, 0.1)};
    s.cardHoverShadow = {25, {0, 8}, rgba(0, 0, 0, 0.15)};
    s.tileShadow = {15, {0, 4}, rgba(88, 142, 151, 0.3)};
    s.warnTileShadow = {15, {0, 4}, rgba(255, 193, 7, 0.3)};
    s.knobShadow = {8, {0, 2}, rgba(0, 0, 0, 0.2)};
}

void applyLightSettings(Theme &t)
{
    const AccentPalette &a = t.accent;
    SettingsPalette &s = t.settings;
    s.subtleFill = rgba(0, 0, 0, 0.04);
    s.subtleHoverFill = rgba(0, 0, 0, 0.08);
    s.subtleBorder = rgba(0, 0, 0, 0.1);
    s.amberSoft = rgba(245, 158, 11, 0.15);
    s.windowFill = t.windowSettings;
    s.sidebarFill = a.white;
    s.sidebarEdge = a.gray200;
    s.headerRule = a.gray200;
    s.navIconSliders = a.amber500;
    s.navIconKeyboard = QColor("#8b5cf6");
    s.navIconLightning = a.rose500;
    s.navIconRefresh = a.sky500;
    s.headerDescText = QColor("#111111");
    s.cardDescText = QColor("#111111");
    s.importFill = a.slate200;
    s.importBorder = a.slate300;
    s.importText = a.slate700;
    s.importHoverFill = a.slate300;
    s.cardFill = a.gray50;
    s.cardBorder = a.gray200;
    s.cardHoverBorder = a.slate300;
    s.tileStart = QColor("#06b6d4");
    s.warnBorder = rgba(255, 193, 7, 0.4);
    s.warnHoverBorder = rgba(255, 193, 7, 0.6);
    s.warnFillStart = rgba(255, 193, 7, 0.08);
    s.warnFillEnd = rgba(255, 193, 7, 0.03);
    s.warnText = a.amber600;
    s.warnHoverShadow = {25, {0, 0}, rgba(255, 193, 7, 0.15)};
    s.trackFill = a.gray100;
    s.trackBorder = a.slate300;
    s.trackOnBorder = a.blue400;
    s.selectBorder = rgba(0, 0, 0, 0.12);
    s.selectListFill = a.white;
    s.selectListText = QColor("#1b1d1e");
    s.keyCapFill = a.gray50;
    s.keyCapBorder = a.gray200;
    s.keyCapHoverFill = rgba(255, 255, 255, 0.1);
    s.keyGlyphFocusFill = a.gray200;
    s.keyShape = a.slate500;
    s.tipText = QColor("#4b5563");
    s.shortcutFill = a.white;
    s.shortcutBorder = a.gray200;
    s.shortcutText = QColor("#111827");
    s.shortcutFocusFill = a.gray100;
    s.checkStart = QColor("#06b6d4");
    s.checkEnd = a.blue500;
    s.modalFill = a.white;
    s.modalBorder = a.gray200;
    s.cancelFill = a.gray100;
    s.cancelBorder = QColor("#d1d5db");
    s.cancelText = QColor("#374151");
    s.modalShadow = {60, {0, 20}, rgba(0, 0, 0, 0.15)};
}

void applyDarkSettings(Theme &t)
{
    const AccentPalette &a = t.accent;
    SettingsPalette &s = t.settings;
    s.subtleFill = rgba(255, 255, 255, 0.06);
    s.subtleHoverFill = rgba(255, 255, 255, 0.1);
    s.subtleBorder = rgba(255, 255, 255, 0.08);
    s.amberSoft = rgba(245, 158, 11, 0.18);
    s.windowFill = t.windowSettings;
    s.sidebarFill = a.darkSurface;
    s.sidebarEdge = rgba(255, 255, 255, 0.1);
    s.headerRule = rgba(255, 255, 255, 0.1);
    s.navIconSliders = a.amber400;
    s.navIconKeyboard = a.purple400;
    s.navIconLightning = a.rose400;
    s.navIconRefresh = a.sky400;
    s.headerDescText = rgba(255, 255, 255, 0.7);
    s.cardDescText = rgba(255, 255, 255, 0.6);
    s.importFill = rgba(255, 255, 255, 0.06);
    s.importBorder = rgba(255, 255, 255, 0.14);
    s.importText = t.fontColor;
    s.importHoverFill = rgba(255, 255, 255, 0.1);
    s.cardFill = a.darkSurface;
    s.cardBorder = rgba(255, 255, 255, 0.05);
    s.cardHoverBorder = rgba(255, 255, 255, 0.2);
    s.tileStart = a.sky400;
    s.warnBorder = rgba(255, 193, 7, 0.3);
    s.warnHoverBorder = rgba(255, 193, 7, 0.5);
    s.warnFillStart = rgba(255, 193, 7, 0.05);
    s.warnFillEnd = rgba(255, 193, 7, 0.02);
    s.warnText = a.warnStart;
    s.warnHoverShadow = {25, {0, 0}, rgba(255, 193, 7, 0.1)};
    s.trackFill = rgba(255, 255, 255, 0.2);
    s.trackBorder = rgba(255, 255, 255, 0.1);
    s.trackOnBorder = t.hoverColor;
    s.selectBorder = rgba(255, 255, 255, 0.15);
    s.selectListFill = QColor("#141618");
    s.selectListText = QColor("#f0f3f6");
    s.keyCapFill = rgba(255, 255, 255, 0.03);
    s.keyCapBorder = rgba(255, 255, 255, 0.05);
    s.keyCapHoverFill = rgba(255, 255, 255, 0.1);
    s.keyGlyphFocusFill = rgba(255, 255, 255, 0.1);
    s.keyShape = rgba(255, 255, 255, 0.3);
    s.tipText = rgba(255, 255, 255, 0.75);
    s.shortcutFill = rgba(255, 255, 255, 0.03);
    s.shortcutBorder = rgba(255, 255, 255, 0.1);
    s.shortcutText = t.fontColor;
    s.shortcutFocusFill = rgba(255, 255, 255, 0.1);
    s.checkStart = a.green500;
    s.checkEnd = a.green600;
    s.modalFill = t.background;
    s.modalBorder = rgba(255, 255, 255, 0.1);
    s.cancelFill = rgba(255, 255, 255, 0.1);
    s.cancelBorder = rgba(255, 255, 255, 0.2);
    s.cancelText = t.fontColor;
    s.modalShadow = {60, {0, 20}, rgba(0, 0, 0, 0.3)};
}

void applyUpdate(Theme &t)
{
    UpdatePalette &u = t.update;
    u.changelogDim = rgba(0, 0, 0, 0.5);
    u.changelogBorder = rgba(255, 255, 255, 0.1);
    u.rule = rgba(255, 255, 255, 0.1);
    u.pillStart = rgba(255, 255, 255, 0.15);
    u.pillEnd = rgba(255, 255, 255, 0.05);
    u.codeFill = rgba(255, 255, 255, 0.1);
    u.promptFill = t.accent.darkSurface;
    u.promptBorder = rgba(255, 255, 255, 0.1);
    u.promptHeading = t.accent.white;
    u.promptVersion = rgba(255, 255, 255, 0.7);
    u.promptBody = rgba(255, 255, 255, 0.8);
    u.laterBorder = rgba(255, 255, 255, 0.2);
    u.laterText = t.accent.white;
    u.versionOld = rgba(255, 255, 255, 0.5);
    u.versionNew = QColor(199, 210, 254);
    u.progressTrack = rgba(255, 255, 255, 0.1);
    u.statusText = rgba(255, 255, 255, 0.7);
    u.statusError = QColor(252, 165, 165);
    u.changelogShadow = {32, {0, 8}, rgba(0, 0, 0, 0.4)};
    u.pillShadow = {15, {0, 4}, rgba(0, 0, 0, 0.2)};
    u.gotItShadow = {8, {0, 2}, rgba(0, 0, 0, 0.2)};
}

void applyEditor(Theme &t, bool dark)
{
    EditorPalette &e = t.editor;
    e.rowHoverFill = rgba(110, 110, 110, 0.1);
    e.rowEditFill = rgba(110, 110, 110, 0.3);
    e.valueHoverFill = rgba(110, 110, 110, 0.2);
    e.holdBannerFill = rgba(59, 130, 246, 0.14);
    e.exportFill = QColor("#f5f6fa");
    e.exportHoverFill = QColor("#e6eaf3");
    e.exportText = QColor("#2c3e50");
    e.actionText = t.accent.white;
    e.glowRing = rgba(37, 99, 235, 0.18);
    e.glowDrop = {10, {0, 4}, rgba(14, 165, 233, 0.28)};
    e.noteGlow = !dark;
    if (dark) {
        e.metaFill = rgba(30, 35, 50, 0.55);
        e.metaBorder = rgba(255, 255, 255, 0.15);
        e.metaShadow = {32, {0, 8}, rgba(0, 0, 0, 0.3)};
    } else {
        e.metaFill = rgba(255, 255, 255, 0.6);
        e.metaBorder = rgba(148, 163, 184, 0.35);
        e.metaShadow = {24, {0, 8}, rgba(15, 23, 42, 0.12)};
    }
}

void applyCore(Theme &t, const CoreColors &core)
{
    if (core.background) {
        t.background = *core.background;
    }
    if (core.fontColor) {
        t.fontColor = *core.fontColor;
        t.borderColor = *core.fontColor;
    }
    if (core.hoverColor) {
        t.hoverColor = *core.hoverColor;
    }
}

void applySections(Theme &t)
{
    t.cardFill = t.background;
    t.cardText = t.fontColor;
    t.footerFill = t.background;
    t.footerText = t.fontColor;
    t.progressPlayed = t.progressGroove;
    t.settings.contentFill = t.background;
    t.titleBarFill = t.windowMain;
    t.titleBarText = t.fontColor;
    t.windowBorder = t.windowMain;
}

Theme makeLight(const CoreColors &core)
{
    Theme t;
    applyShared(t);
    const AccentPalette &a = t.accent;
    t.kind = AppTheme::Light;
    t.background = QColor("#ffffff");
    t.surfaceDeep = QColor("#0f172a");
    t.fontColor = QColor("#2c3e50");
    t.hoverColor = QColor("#3b83f6");
    t.borderColor = QColor("#2c3e50");
    t.successColor = QColor("#27ae60");
    t.errorColor = QColor("#e74c3c");
    t.gridColor = rgba(100, 116, 139, 0.14);
    t.dotColor = rgba(71, 85, 105, 0.42);
    t.gridNoteColor = QColor("#2563eb");
    t.gridHoverColor = rgba(59, 130, 246, 0.18);
    t.activeColor = rgba(59, 130, 246, 0.24);
    t.editBg = rgba(44, 62, 80, 0.10);
    applyCore(t, core);
    t.windowMain = QColor("#ffffff");
    t.windowSettings = QColor("#f7f9fc");
    t.windowEditor = QColor("#ffffff");
    t.prePaint = QColor("#0a1930");
    t.searchBg = a.gray100;
    t.searchText = a.ink900;
    t.searchPlaceholder = a.slate500;
    t.searchIcon = a.slate500;
    t.cardTopEdge = a.gray200;
    t.cardTopEdgeHeight = 5;
    t.footerTopEdge = a.gray200;
    t.cardLabelTint = a.slate700;
    t.authorIcon = a.sky500;
    t.transcriberIcon = a.amber500;
    t.bpmIcon = a.emerald500;
    t.actionGlyph = withAlpha(t.fontColor, 0.72);
    t.neutralBorder = rgba(15, 23, 42, 0.22);
    t.transportFill = a.white;
    t.transportSkip = a.sky500;
    t.transportSkipHover = a.sky600;
    t.progressGroove = a.slate400;
    t.progressHandle = a.blue500;
    t.speedFieldBg = a.gray100;
    t.speedFieldText = a.ink900;
    t.switchTrackOff = a.gray200;
    t.switchKnob = a.slate400;
    t.loopIcon = a.amber500;
    t.loopIconHover = a.amber600;
    t.scrollHandle = a.slate400;
    t.scrollHandleHover = a.slate400;
    t.scrollGroove = a.gray200;
    t.toastText = a.ink900;
    t.toastTextShadow = a.transparent;
    t.popoverFill = a.white;
    t.popoverBorder = rgba(15, 23, 42, 0.1);
    t.heartOn = a.red600;
    t.heartOff = QColor("#bebebe");
    t.drawerFill = a.white;
    t.navIdleText = rgba(0, 0, 0, 0.6);
    t.navHoverText = rgba(0, 0, 0, 0.8);
    t.navHoverFill = rgba(0, 0, 0, 0.05);
    t.navActiveStart = a.blue500;
    t.navActiveEnd = a.blue700;
    t.bulkFill = rgba(255, 255, 255, 0.94);
    t.pickerBorder = rgba(15, 23, 42, 0.14);
    t.storeBadgeFill = rgba(59, 130, 246, 0.1);
    t.storeBadgeText = a.ink900;
    t.shadowFooter = {2, {0, 1}, rgba(16, 24, 40, 0.05)};
    t.shadowPopover = {36, {0, 18}, rgba(15, 23, 42, 0.16)};
    applySharedSettings(t);
    applyLightSettings(t);
    applyEditor(t, false);
    applyUpdate(t);
    applySections(t);
    return t;
}

Theme makeDark(const CoreColors &core)
{
    Theme t;
    applyShared(t);
    const AccentPalette &a = t.accent;
    t.kind = AppTheme::Dark;
    t.background = QColor("#171717");
    t.surfaceDeep = QColor("#05012c");
    t.fontColor = QColor("#a8b2d1");
    t.hoverColor = QColor("#588e97");
    t.borderColor = QColor("#a8b2d1");
    t.successColor = QColor("#4CAF50");
    t.errorColor = QColor("#f44336");
    t.gridColor = rgba(110, 110, 110, 0.20);
    t.dotColor = rgba(255, 255, 255, 0.30);
    t.gridNoteColor = rgba(255, 255, 255, 0.80);
    t.gridHoverColor = QColor("#588e97");
    t.activeColor = rgba(110, 110, 110, 0.40);
    t.editBg = rgba(0, 0, 0, 0.20);
    applyCore(t, core);
    t.windowMain = QColor("#1B1D1E");
    t.windowSettings = QColor("#1B1D1E");
    t.windowEditor = QColor("#171717");
    t.prePaint = QColor("#171717");
    t.searchBg = QColor("#171717");
    t.searchText = a.white;
    t.searchPlaceholder = a.white;
    t.searchIcon = t.fontColor;
    t.cardTopEdge = a.black;
    t.cardTopEdgeHeight = 1;
    t.footerTopEdge = a.black;
    t.cardLabelTint = t.fontColor;
    t.authorIcon = a.sky400;
    t.transcriberIcon = a.amber400;
    t.bpmIcon = a.emerald400;
    t.actionGlyph = withAlpha(t.fontColor, 0.72);
    t.neutralBorder = rgba(148, 163, 184, 0.4);
    t.transportFill = a.transparent;
    t.transportSkip = a.sky400;
    t.transportSkipHover = a.sky500;
    t.progressGroove = t.fontColor;
    t.progressHandle = t.hoverColor;
    t.speedFieldBg = t.background;
    t.speedFieldText = t.fontColor;
    t.switchTrackOff = t.surfaceDeep;
    t.switchKnob = t.fontColor;
    t.loopIcon = a.amber400;
    t.loopIconHover = a.amber500;
    t.scrollHandle = rgba(255, 255, 255, 0.20);
    t.scrollHandleHover = rgba(255, 255, 255, 0.30);
    t.scrollGroove = rgba(255, 255, 255, 0.05);
    t.toastText = a.white;
    t.toastTextShadow = rgba(0, 0, 0, 0.25);
    t.popoverFill = t.background;
    t.popoverBorder = rgba(148, 163, 184, 0.22);
    t.heartOn = a.red500;
    t.heartOff = rgba(255, 255, 255, 0.6);
    t.drawerFill = t.background;
    t.navIdleText = rgba(255, 255, 255, 0.7);
    t.navHoverText = rgba(255, 255, 255, 0.9);
    t.navHoverFill = a.transparent;
    t.navActiveStart = t.hoverColor;
    t.navActiveEnd = a.gradientEnd;
    t.bulkFill = withAlpha(t.background, 0.92);
    t.pickerBorder = rgba(255, 255, 255, 0.15);
    t.storeBadgeFill = rgba(59, 130, 246, 0.15);
    t.storeBadgeText = t.fontColor;
    t.confirmFill = rgba(32, 34, 36, 0.97);
    t.confirmMessageFill = rgba(255, 255, 255, 0.04);
    t.confirmText = t.fontColor;
    t.confirmRule = rgba(255, 255, 255, 0.08);
    t.confirmButtonHover = rgba(255, 255, 255, 0.06);
    t.shadowFooter = t.shadowCard;
    t.shadowPopover = {40, {0, 18}, rgba(0, 0, 0, 0.24)};
    applySharedSettings(t);
    applyDarkSettings(t);
    applyEditor(t, true);
    applyUpdate(t);
    applySections(t);
    return t;
}

}  // namespace

QString cssColor(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(color.alphaF() * alpha);
    return color;
}

QColor blend(const QColor &from, const QColor &to, qreal amount)
{
    const auto mix = [amount](int a, int b) { return qRound(a + (b - a) * amount); };
    return QColor(mix(from.red(), to.red()), mix(from.green(), to.green()), mix(from.blue(), to.blue()),
                  mix(from.alpha(), to.alpha()));
}

Theme Theme::build(AppTheme kind, const CoreColors &core)
{
    return kind == AppTheme::Dark ? makeDark(core) : makeLight(core);
}

const Theme &Theme::light()
{
    static const Theme theme = makeLight({});
    return theme;
}

const Theme &Theme::dark()
{
    static const Theme theme = makeDark({});
    return theme;
}

const Theme &Theme::forKind(AppTheme kind)
{
    return kind == AppTheme::Dark ? dark() : light();
}

QColor Theme::toastBase(int type) const
{
    switch (type) {
    case 1:
        return toastSuccess;
    case 2:
        return toastWarning;
    case 3:
        return toastError;
    case 4:
        return toastInfo;
    default:
        return toastNeutral;
    }
}

QMap<QString, QString> Theme::qssTokens() const
{
    const AccentPalette &a = accent;
    QMap<QString, QString> t;
    t["background"] = cssColor(background);
    t["fontColor"] = cssColor(fontColor);
    t["hoverColor"] = cssColor(hoverColor);
    t["windowMain"] = cssColor(windowMain);
    t["windowSettings"] = cssColor(windowSettings);
    t["windowEditor"] = cssColor(windowEditor);
    t["searchBg"] = cssColor(searchBg);
    t["searchText"] = cssColor(searchText);
    t["searchPlaceholder"] = cssColor(searchPlaceholder);
    t["neutralBorder"] = cssColor(neutralBorder);
    t["neutralBorderDisabled"] = cssColor(neutralBorderDisabled);
    t["neutralFillDisabled"] = cssColor(neutralFillDisabled);
    t["transportFill"] = cssColor(transportFill);
    t["transportSkip"] = cssColor(transportSkip);
    t["transportSkipHover"] = cssColor(transportSkipHover);
    t["transportPlay"] = cssColor(transportPlay);
    t["transportPlayHover"] = cssColor(transportPlayHover);
    t["progressGroove"] = cssColor(progressGroove);
    t["progressHandle"] = cssColor(progressHandle);
    t["speedFieldBg"] = cssColor(speedFieldBg);
    t["speedFieldText"] = cssColor(speedFieldText);
    t["scrollHandle"] = cssColor(scrollHandle);
    t["scrollHandleHover"] = cssColor(scrollHandleHover);
    t["scrollGroove"] = cssColor(scrollGroove);
    t["fontColor75"] = cssColor(withAlpha(fontColor, 0.75));
    t["settingsWindowFill"] = cssColor(settings.windowFill);
    t["settingsHeaderDesc"] = cssColor(settings.headerDescText);
    t["settingsCardDesc"] = cssColor(settings.cardDescText);
    t["settingsWarnText"] = cssColor(withAlpha(settings.warnText, 0.9));
    t["settingsTipText"] = cssColor(settings.tipText);
    t["settingsShortcutFill"] = cssColor(settings.shortcutFill);
    t["settingsShortcutBorder"] = cssColor(settings.shortcutBorder);
    t["settingsShortcutText"] = cssColor(settings.shortcutText);
    t["settingsShortcutFocusFill"] = cssColor(settings.shortcutFocusFill);
    t["settingsShortcutFocusBorder"] = cssColor(settings.shortcutFocusBorder);
    t["editorRowHover"] = cssColor(editor.rowHoverFill);
    t["editorHoldFill"] = cssColor(editor.holdBannerFill);
    t["editorExportFill"] = cssColor(editor.exportFill);
    t["editorExportHover"] = cssColor(editor.exportHoverFill);
    t["editorExportText"] = cssColor(editor.exportText);
    t["editorActionText"] = cssColor(editor.actionText);
    t["successColor"] = cssColor(successColor);
    t["errorColor"] = cssColor(errorColor);
    t["borderColor"] = cssColor(borderColor);
    t["editBg"] = cssColor(editBg);
    t["updatePromptHeading"] = cssColor(update.promptHeading);
    t["updatePromptVersion"] = cssColor(update.promptVersion);
    t["updateVersionOld"] = cssColor(update.versionOld);
    t["updateVersionNew"] = cssColor(update.versionNew);
    t["updateStatusText"] = cssColor(update.statusText);
    t["updateStatusError"] = cssColor(update.statusError);
    t["successHover"] = cssColor(withAlpha(successColor, 0.9));
    t["errorHover"] = cssColor(withAlpha(errorColor, 0.9));
    t["successDisabled"] = cssColor(withAlpha(successColor, 0.55));
    t["errorDisabled"] = cssColor(withAlpha(errorColor, 0.55));
    t["editorActionDisabledText"] = cssColor(withAlpha(editor.actionText, 0.55));
    t["pickerFieldBorder"] = cssColor(pickerFieldBorder);
    t["pickerFieldFocus"] = cssColor(pickerFieldFocus);
    t["pickerRowRule"] = cssColor(pickerRowRule);
    t["confirmText"] = cssColor(confirmText);
    t["confirmOk"] = cssColor(confirmOk);
    t["confirmCancel"] = cssColor(confirmCancel);
    t["confirmButtonHover"] = cssColor(confirmButtonHover);
    t["confirmRule"] = cssColor(confirmRule);
    t["popoverFill"] = cssColor(popoverFill);
    t["primaryFill"] = cssColor(a.blue600);
    t["destructiveFill"] = cssColor(a.red700);
    t["white"] = cssColor(a.white);
    t["transparent"] = cssColor(a.transparent);
    t["settingsDivider"] = cssColor(settings.headerRule);
    t["settingsContentFill"] = cssColor(settings.contentFill);
    t["footerText"] = cssColor(footerText);
    t["progressPlayed"] = cssColor(progressPlayed);
    return t;
}

}  // namespace sky
