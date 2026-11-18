#include "ui/theme_catalog.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace sky {

namespace {

using FieldRef = std::function<QColor &(Theme &)>;

QColor hoverShade(const QColor &color)
{
    return color.darker(112);
}

ThemeSlot slot(const char *id, const char *label, FieldRef field)
{
    ThemeSlot result;
    result.id = QString::fromLatin1(id);
    result.label = QString::fromUtf8(label);
    result.read = [field](const Theme &theme) { return field(const_cast<Theme &>(theme)); };
    result.write = [field](Theme &theme, const QColor &color) { field(theme) = color; };
    return result;
}

ThemeSlot slot(const char *id, const char *label, FieldRef field, std::function<void(Theme &, const QColor &)> also)
{
    ThemeSlot result = slot(id, label, field);
    result.write = [field, also = std::move(also)](Theme &theme, const QColor &color) {
        field(theme) = color;
        also(theme, color);
    };
    return result;
}

ThemeSlot coreSlot(const char *id, const char *label, FieldRef field)
{
    ThemeSlot result = slot(id, label, std::move(field));
    result.core = true;
    return result;
}

ThemeSection section(const char *id, const char *title, const char *description, const char *iconId,
                     std::vector<ThemeSlot> entries, PreviewScene scene = PreviewScene::Main)
{
    return ThemeSection{QString::fromLatin1(id), QString::fromUtf8(title), QString::fromUtf8(description),
                        QString::fromLatin1(iconId), std::move(entries), scene};
}

std::vector<ThemeSection> makeSections()
{
    std::vector<ThemeSection> sections;
    sections.push_back(section(
        "app", "Everyday Colors", "The base colors most of the app is built from.", "palette",
        {
            coreSlot("app.surface", "Base surface", [](Theme &t) -> QColor & { return t.background; }),
            coreSlot("app.text", "Text", [](Theme &t) -> QColor & { return t.fontColor; }),
            coreSlot("app.accent", "Accent", [](Theme &t) -> QColor & { return t.hoverColor; }),
            slot("app.success", "Success", [](Theme &t) -> QColor & { return t.successColor; }),
            slot("app.error", "Error", [](Theme &t) -> QColor & { return t.errorColor; }),
        }));
    sections.push_back(section(
        "main", "Main Window", "The song library window behind everything else.", "music-note-list",
        {
            slot("main.window", "Window background", [](Theme &t) -> QColor & { return t.windowMain; },
                 [](Theme &t, const QColor &c) { t.prePaint = c; }),
            slot("main.searchFill", "Search box", [](Theme &t) -> QColor & { return t.searchBg; }),
            slot("main.searchText", "Search text", [](Theme &t) -> QColor & { return t.searchText; },
                 [](Theme &t, const QColor &c) { t.searchPlaceholder = withAlpha(c, 0.7); }),
            slot("main.searchIcon", "Search icon", [](Theme &t) -> QColor & { return t.searchIcon; }),
            slot("main.buttonBorder", "Button border", [](Theme &t) -> QColor & { return t.neutralBorder; }),
            slot("main.scrollbar", "Scrollbar", [](Theme &t) -> QColor & { return t.scrollHandle; },
                 [](Theme &t, const QColor &c) { t.scrollHandleHover = c; }),
        }));
    sections.push_back(section(
        "title", "Title Bar", "The bar at the top of every window, with the close button.", "window",
        {
            slot("title.fill", "Background", [](Theme &t) -> QColor & { return t.titleBarFill; }),
            slot("title.text", "Title text", [](Theme &t) -> QColor & { return t.titleBarText; }),
            slot("title.border", "Window border", [](Theme &t) -> QColor & { return t.windowBorder; }),
        }));
    sections.push_back(section(
        "card", "Song Cards", "Cards for songs, playlists and the store.", "music-note-beamed",
        {
            slot("card.fill", "Card background", [](Theme &t) -> QColor & { return t.cardFill; }),
            slot("card.text", "Title and values", [](Theme &t) -> QColor & { return t.cardText; }),
            slot("card.label", "Labels", [](Theme &t) -> QColor & { return t.cardLabelTint; }),
            slot("card.edge", "Top edge", [](Theme &t) -> QColor & { return t.cardTopEdge; }),
            slot("card.author", "Author icon", [](Theme &t) -> QColor & { return t.authorIcon; }),
            slot("card.transcriber", "Transcriber icon", [](Theme &t) -> QColor & { return t.transcriberIcon; }),
            slot("card.bpm", "BPM icon", [](Theme &t) -> QColor & { return t.bpmIcon; }),
            slot("card.hold", "Hold badge", [](Theme &t) -> QColor & { return t.holdBadgeFill; }),
            slot("card.menu", "Menu button", [](Theme &t) -> QColor & { return t.actionGlyph; }),
            slot("card.selected", "Selected outline", [](Theme &t) -> QColor & { return t.cardSelectedOutline; },
                 [](Theme &t, const QColor &c) { t.cardSelectedTint = c; }),
            slot("card.favorite", "Favorite heart", [](Theme &t) -> QColor & { return t.heartOn; }),
            slot("card.storeTag", "Store tags", [](Theme &t) -> QColor & { return t.storeBadgeFill; }),
        }));
    sections.push_back(section(
        "footer", "Player Bar", "The bar at the bottom with the song, progress and buttons.", "play-circle",
        {
            slot("footer.fill", "Bar background", [](Theme &t) -> QColor & { return t.footerFill; }),
            slot("footer.text", "Text and icons", [](Theme &t) -> QColor & { return t.footerText; }),
            slot("footer.edge", "Top edge", [](Theme &t) -> QColor & { return t.footerTopEdge; }),
            slot("footer.track", "Progress track", [](Theme &t) -> QColor & { return t.progressGroove; }),
            slot("footer.played", "Progress played", [](Theme &t) -> QColor & { return t.progressPlayed; }),
            slot("footer.handle", "Progress handle", [](Theme &t) -> QColor & { return t.progressHandle; }),
            slot("footer.play", "Play button", [](Theme &t) -> QColor & { return t.transportPlay; },
                 [](Theme &t, const QColor &c) { t.transportPlayHover = hoverShade(c); }),
            slot("footer.skip", "Previous and next", [](Theme &t) -> QColor & { return t.transportSkip; },
                 [](Theme &t, const QColor &c) { t.transportSkipHover = hoverShade(c); }),
            slot("footer.buttonFill", "Button fill", [](Theme &t) -> QColor & { return t.transportFill; }),
            slot("footer.loop", "Loop icon", [](Theme &t) -> QColor & { return t.loopIcon; },
                 [](Theme &t, const QColor &c) { t.loopIconHover = hoverShade(c); }),
            slot("footer.speedFill", "Speed box", [](Theme &t) -> QColor & { return t.speedFieldBg; }),
            slot("footer.speedText", "Speed text", [](Theme &t) -> QColor & { return t.speedFieldText; }),
            slot("footer.switch", "Switch track", [](Theme &t) -> QColor & { return t.switchTrackOff; }),
            slot("footer.knob", "Switch knob", [](Theme &t) -> QColor & { return t.switchKnob; }),
        }));
    sections.push_back(section(
        "drawer", "Menu Drawer", "The side menu with All Songs, Favorite, Playlists and Store.", "list",
        {
            slot("drawer.fill", "Drawer background", [](Theme &t) -> QColor & { return t.drawerFill; }),
            slot("drawer.text", "Tab text", [](Theme &t) -> QColor & { return t.navIdleText; },
                 [](Theme &t, const QColor &c) { t.navHoverText = c; }),
            slot("drawer.activeStart", "Active tab", [](Theme &t) -> QColor & { return t.navActiveStart; }),
            slot("drawer.selectionBar", "Selection bar", [](Theme &t) -> QColor & { return t.bulkFill; }),
        }));
    sections.push_back(section(
        "popup", "Menus and Dialogs", "Pop-up menus, the playlist picker and confirm boxes.", "three-dots-vertical",
        {
            slot("popup.menuFill", "Menu background", [](Theme &t) -> QColor & { return t.popoverFill; }),
            slot("popup.menuBorder", "Menu border", [](Theme &t) -> QColor & { return t.popoverBorder; }),
            slot("popup.menuHover", "Menu highlight", [](Theme &t) -> QColor & { return t.menuHoverText; },
                 [](Theme &t, const QColor &c) { t.menuHoverFill = withAlpha(c, 0.12); }),
            slot("popup.dialogFill", "Dialog background", [](Theme &t) -> QColor & { return t.confirmFill; }),
            slot("popup.dialogText", "Dialog text", [](Theme &t) -> QColor & { return t.confirmText; }),
            slot("popup.confirm", "Confirm button", [](Theme &t) -> QColor & { return t.confirmOk; }),
            slot("popup.dim", "Dim behind pop-ups", [](Theme &t) -> QColor & { return t.overlayDim; },
                 [](Theme &t, const QColor &c) { t.confirmOverlay = c; }),
        },
        PreviewScene::Popups));
    sections.push_back(section(
        "toast", "Notifications", "The small messages that slide in at the top.", "info-circle",
        {
            slot("toast.success", "Success", [](Theme &t) -> QColor & { return t.toastSuccess; }),
            slot("toast.warning", "Warning", [](Theme &t) -> QColor & { return t.toastWarning; }),
            slot("toast.error", "Error", [](Theme &t) -> QColor & { return t.toastError; }),
            slot("toast.info", "Info", [](Theme &t) -> QColor & { return t.toastInfo; }),
            slot("toast.text", "Text", [](Theme &t) -> QColor & { return t.toastText; }),
        },
        PreviewScene::Alerts));
    sections.push_back(section(
        "settings", "Settings Window", "This window: sidebar, pages and cards.", "gear-fill",
        {
            slot("settings.content", "Page background", [](Theme &t) -> QColor & { return t.settings.contentFill; }),
            slot("settings.sidebar", "Sidebar", [](Theme &t) -> QColor & { return t.settings.sidebarFill; }),
            slot("settings.sidebarEdge", "Sidebar edge", [](Theme &t) -> QColor & { return t.settings.sidebarEdge; }),
            slot("settings.card", "Cards", [](Theme &t) -> QColor & { return t.settings.cardFill; }),
            slot("settings.cardBorder", "Card border", [](Theme &t) -> QColor & { return t.settings.cardBorder; }),
            slot("settings.description", "Descriptions", [](Theme &t) -> QColor & { return t.settings.cardDescText; },
                 [](Theme &t, const QColor &c) { t.settings.headerDescText = c; }),
            slot("settings.switch", "Switch on", [](Theme &t) -> QColor & { return t.settings.checkStart; },
                 [](Theme &t, const QColor &c) { t.settings.checkEnd = hoverShade(c); }),
            slot("settings.save", "Save button", [](Theme &t) -> QColor & { return t.settings.saveStart; },
                 [](Theme &t, const QColor &c) { t.settings.saveEnd = hoverShade(c); }),
        },
        PreviewScene::Settings));
    sections.push_back(section(
        "editor", "Sheet Editor", "The window for editing a song's notes.", "sheet-editor",
        {
            slot("editor.window", "Window background", [](Theme &t) -> QColor & { return t.windowEditor; }),
            slot("editor.panel", "Info panel", [](Theme &t) -> QColor & { return t.editor.metaFill; }),
            slot("editor.grid", "Grid lines", [](Theme &t) -> QColor & { return t.gridColor; }),
            slot("editor.dot", "Grid dots", [](Theme &t) -> QColor & { return t.dotColor; }),
            slot("editor.note", "Notes", [](Theme &t) -> QColor & { return t.gridNoteColor; }),
            slot("editor.hover", "Hovered cell", [](Theme &t) -> QColor & { return t.gridHoverColor; }),
            slot("editor.active", "Active cell", [](Theme &t) -> QColor & { return t.activeColor; }),
            slot("editor.export", "Export button", [](Theme &t) -> QColor & { return t.editor.exportFill; },
                 [](Theme &t, const QColor &c) { t.editor.exportHoverFill = hoverShade(c); }),
        },
        PreviewScene::Editor));
    return sections;
}

QuickColor quick(const char *id, const char *label, const char *description, const char *primary,
                 std::vector<std::pair<const char *, std::function<QColor(const QColor &)>>> targets)
{
    QuickColor result{QString::fromLatin1(id), QString::fromUtf8(label), QString::fromUtf8(description),
                      QString::fromLatin1(primary), {}};
    for (auto &[slotId, derive] : targets) {
        result.targets.emplace_back(QString::fromLatin1(slotId), std::move(derive));
    }
    return result;
}

std::vector<QuickColor> makeQuickColors();

QuickColorSet quickSet(PreviewScene scene, const char *name, const char *description, std::vector<QuickColor> colors)
{
    return QuickColorSet{scene, QString::fromUtf8(name), QString::fromUtf8(description), std::move(colors)};
}

std::vector<QuickColorSet> makeQuickColorSets()
{
    const auto same = [](const QColor &c) { return c; };
    const auto soft = [](qreal alpha) { return [alpha](const QColor &c) { return withAlpha(c, alpha); }; };
    std::vector<QuickColorSet> sets;
    sets.push_back(quickSet(PreviewScene::Main, "Song List", "Pick these five and the rest of the app follows.",
                            makeQuickColors()));
    sets.push_back(quickSet(
        PreviewScene::Popups, "Menus", "The main colors of pop-up menus and confirm boxes.",
        {quick("popupFill", "Pop-up background", "Behind pop-up menus and confirm boxes.", "popup.menuFill",
               {{"popup.menuFill", same}, {"popup.dialogFill", same}}),
         quick("popupText", "Pop-up text", "The words in confirm boxes.", "popup.dialogText",
               {{"popup.dialogText", same}}),
         quick("popupHighlight", "Highlight", "The menu row under the mouse and the confirm button.", "popup.menuHover",
               {{"popup.menuHover", same}, {"popup.confirm", same}}),
         quick("popupBorder", "Border", "The thin line around pop-up menus.", "popup.menuBorder",
               {{"popup.menuBorder", same}}),
         quick("popupDim", "Dim", "The shade over the app behind a pop-up.", "popup.dim", {{"popup.dim", same}})}));
    sets.push_back(quickSet(
        PreviewScene::Alerts, "Alerts", "The main colors of the messages that slide in at the top.",
        {quick("alertSuccess", "Success", "Shown when something worked.", "toast.success", {{"toast.success", same}}),
         quick("alertWarning", "Warning", "Shown when something needs a look.", "toast.warning",
               {{"toast.warning", same}}),
         quick("alertError", "Error", "Shown when something went wrong.", "toast.error", {{"toast.error", same}}),
         quick("alertInfo", "Info", "Shown for progress and tips.", "toast.info", {{"toast.info", same}}),
         quick("alertText", "Text", "The words in every message.", "toast.text", {{"toast.text", same}})}));
    sets.push_back(quickSet(
        PreviewScene::Settings, "Settings", "The main colors of the Settings window.",
        {quick("settingsBackground", "Background", "Behind the pages and the sidebar.", "settings.content",
               {{"settings.content", same}, {"settings.sidebar", same}}),
         quick("settingsCards", "Cards", "The boxes each setting sits in.", "settings.card", {{"settings.card", same}}),
         quick("settingsBorders", "Borders", "Lines around cards and next to the sidebar.", "settings.cardBorder",
               {{"settings.cardBorder", same}, {"settings.sidebarEdge", same}}),
         quick("settingsDescriptions", "Descriptions", "The smaller text under each title.", "settings.description",
               {{"settings.description", same}}),
         quick("settingsSwitches", "Switches and Save", "Switches that are on, and the Save button.", "settings.switch",
               {{"settings.switch", same}, {"settings.save", same}})}));
    sets.push_back(quickSet(
        PreviewScene::Editor, "Sheet Editor", "The main colors of the window for editing notes.",
        {quick("editorBackground", "Background", "Behind the notes and the key pad.", "editor.window",
               {{"editor.window", same}}),
         quick("editorCells", "Cells", "The boxes for each moment, and the keys.", "editor.grid",
               {{"editor.grid", same}}),
         quick("editorNotes", "Notes", "The squares for keys that play.", "editor.note", {{"editor.note", same}}),
         quick("editorHighlights", "Highlighted cells", "The cell under the mouse and the chosen one.", "editor.active",
               {{"editor.active", same}, {"editor.hover", soft(0.7)}}),
         quick("editorPanel", "Info panel", "The box with the song name and BPM.", "editor.panel",
               {{"editor.panel", same}})}));
    return sets;
}

std::vector<QuickColor> makeQuickColors()
{
    const auto same = [](const QColor &c) { return c; };
    const auto soft = [](qreal alpha) { return [alpha](const QColor &c) { return withAlpha(c, alpha); }; };
    std::vector<QuickColor> list;
    list.push_back(quick("accent", "Accent", "Buttons, highlights, progress and the active tab.", "app.accent",
                         {{"app.accent", same},
                          {"footer.played", same},
                          {"footer.handle", same},
                          {"drawer.activeStart", same},
                          {"card.selected", same},
                          {"popup.menuHover", same},
                          {"editor.note", same},
                          {"settings.switch", same}}));
    list.push_back(quick("background", "Background", "Behind the song list, Settings and the editor.", "main.window",
                         {{"main.window", same},
                          {"editor.window", same},
                          {"settings.content", same},
                          {"settings.sidebar", same},
                          {"drawer.fill", same}}));
    list.push_back(quick("cards", "Cards and boxes", "Song cards, menus, the search box and Settings cards.", "card.fill",
                         {{"app.surface", same},
                          {"card.fill", same},
                          {"settings.card", same},
                          {"popup.menuFill", same},
                          {"main.searchFill", same},
                          {"footer.speedFill", same}}));
    list.push_back(quick("text", "Text", "Titles, labels and everything you read.", "app.text",
                         {{"app.text", same},
                          {"card.text", same},
                          {"footer.text", same},
                          {"main.searchText", same},
                          {"footer.speedText", same},
                          {"card.label", soft(0.8)},
                          {"settings.description", soft(0.7)}}));
    list.push_back(quick("player", "Player bar", "The bar at the bottom with the play buttons.", "footer.fill",
                         {{"footer.fill", same}}));
    return list;
}

using ColorList = std::initializer_list<std::pair<const char *, const char *>>;

ThemeColors toColors(ColorList colors)
{
    ThemeColors result;
    for (const auto &[slotId, value] : colors) {
        result[QString::fromLatin1(slotId)] = QColor(QString::fromLatin1(value));
    }
    return result;
}

ThemeDefinition preset(const char *id, const char *name, ColorList light, ColorList dark)
{
    ThemeDefinition definition;
    definition.id = QString::fromLatin1(id);
    definition.name = QString::fromUtf8(name);
    definition.light = toColors(light);
    definition.dark = toColors(dark);
    definition.builtIn = true;
    return definition;
}

std::vector<ThemeDefinition> makeBuiltIns()
{
    std::vector<ThemeDefinition> themes;
    themes.push_back(preset(kDefaultThemeId, "Default", {}, {}));
    themes.push_back(preset(
        "builtin-midnight", "Indigo",
        {{"app.surface", "#f6f7ff"},       {"app.text", "#2a2f5c"},        {"app.accent", "#5b5fe0"},
         {"main.window", "#eef0ff"},       {"main.searchFill", "#e3e6ff"}, {"main.searchText", "#2a2f5c"},
         {"main.scrollbar", "#c3c8f5"},    {"card.fill", "#ffffff"},       {"card.edge", "#d9ddff"},
         {"card.label", "#4a4f86"},        {"footer.fill", "#ffffff"},     {"footer.edge", "#d9ddff"},
         {"footer.track", "#c3c8f5"},      {"footer.played", "#5b5fe0"},   {"footer.handle", "#4338ca"},
         {"footer.speedFill", "#e3e6ff"},  {"footer.speedText", "#2a2f5c"}, {"drawer.fill", "#f6f7ff"},
         {"drawer.activeStart", "#6366f1"}, {"settings.content", "#f6f7ff"},
         {"settings.sidebar", "#ffffff"},  {"settings.card", "#eef0ff"},   {"settings.cardBorder", "#d9ddff"},
         {"editor.note", "#5b5fe0"}},
        {{"app.surface", "#141a2e"},       {"app.text", "#c7d2fe"},        {"app.accent", "#818cf8"},
         {"main.window", "#0f1424"},       {"main.searchFill", "#1b2340"}, {"card.fill", "#18203a"},
         {"card.edge", "#0b0f1c"},         {"footer.fill", "#121933"},     {"footer.edge", "#0b0f1c"},
         {"footer.track", "#3a4470"},      {"footer.played", "#818cf8"},   {"footer.handle", "#a5b4fc"},
         {"footer.speedFill", "#1b2340"},  {"drawer.fill", "#121933"},     {"drawer.activeStart", "#6366f1"},
         {"popup.menuFill", "#18203a"},  {"settings.content", "#141a2e"},
         {"settings.sidebar", "#121933"},  {"settings.card", "#18203a"},   {"editor.window", "#141a2e"}}));
    themes.push_back(preset(
        "builtin-forest", "Green",
        {{"app.surface", "#f6faf6"},       {"app.text", "#24402c"},        {"app.accent", "#2f8a4a"},
         {"main.window", "#eef6ef"},       {"main.searchFill", "#e0efe3"}, {"main.searchText", "#24402c"},
         {"main.scrollbar", "#b5d6bd"},    {"card.fill", "#ffffff"},       {"card.edge", "#cfe5d4"},
         {"card.label", "#3d6147"},        {"card.author", "#1f9e86"},     {"footer.fill", "#ffffff"},
         {"footer.edge", "#cfe5d4"},       {"footer.track", "#b5d6bd"},    {"footer.played", "#2f8a4a"},
         {"footer.handle", "#1f6e38"},     {"footer.speedFill", "#e0efe3"}, {"footer.speedText", "#24402c"},
         {"drawer.fill", "#f6faf6"},       {"drawer.activeStart", "#3f8f55"}, 
         {"settings.content", "#f6faf6"},  {"settings.sidebar", "#ffffff"}, {"settings.card", "#eef6ef"},
         {"settings.cardBorder", "#cfe5d4"}, {"editor.note", "#2f8a4a"}},
        {{"app.surface", "#17201b"},       {"app.text", "#c8dccb"},        {"app.accent", "#6fbf73"},
         {"main.window", "#121a15"},       {"main.searchFill", "#1d2921"}, {"card.fill", "#1b261f"},
         {"card.edge", "#0c120e"},         {"card.author", "#7dd3c0"},     {"footer.fill", "#16201a"},
         {"footer.edge", "#0c120e"},       {"footer.track", "#3c5244"},    {"footer.played", "#6fbf73"},
         {"footer.handle", "#9be09f"},     {"footer.speedFill", "#1d2921"}, {"drawer.fill", "#16201a"},
         {"drawer.activeStart", "#3f8f55"}, {"popup.menuFill", "#1b261f"},
         {"settings.content", "#17201b"},  {"settings.sidebar", "#16201a"}, {"settings.card", "#1b261f"},
         {"editor.window", "#17201b"}}));
    themes.push_back(preset(
        "builtin-sunset", "Orange",
        {{"app.surface", "#fff8f3"},       {"app.text", "#4a2c2a"},        {"app.accent", "#e8643c"},
         {"main.window", "#fff1e8"},       {"main.searchFill", "#ffe4d4"}, {"main.searchText", "#4a2c2a"},
         {"main.scrollbar", "#f7c0a6"},    {"card.fill", "#ffffff"},       {"card.edge", "#ffd3bd"},
         {"card.label", "#7a4a3f"},        {"footer.fill", "#ffffff"},     {"footer.edge", "#ffd3bd"},
         {"footer.track", "#f7c0a6"},      {"footer.played", "#e8643c"},   {"footer.handle", "#c2410c"},
         {"footer.speedFill", "#ffe4d4"},  {"footer.speedText", "#4a2c2a"}, {"drawer.fill", "#fff8f3"},
         {"drawer.activeStart", "#ff6b6b"}, {"settings.content", "#fff8f3"},
         {"settings.sidebar", "#ffffff"},  {"settings.card", "#fff1e8"},   {"settings.cardBorder", "#ffd3bd"},
         {"editor.note", "#e8643c"}},
        {{"app.surface", "#221a26"},       {"app.text", "#f3d9c9"},        {"app.accent", "#ff8a5c"},
         {"main.window", "#1c1520"},       {"main.searchFill", "#2c2130"}, {"card.fill", "#2a1f2e"},
         {"card.edge", "#120c14"},         {"footer.fill", "#241b28"},     {"footer.edge", "#120c14"},
         {"footer.track", "#5a4552"},      {"footer.played", "#ff8a5c"},   {"footer.handle", "#ffb38a"},
         {"footer.speedFill", "#2c2130"},  {"drawer.fill", "#241b28"},     {"drawer.activeStart", "#ff6b6b"},
         {"popup.menuFill", "#2a1f2e"},  {"settings.content", "#221a26"},
         {"settings.sidebar", "#241b28"},  {"settings.card", "#2a1f2e"},   {"editor.window", "#221a26"}}));
    themes.push_back(preset(
        "builtin-sakura", "Pink",
        {{"app.surface", "#fff7fa"},       {"app.text", "#5b3a4a"},        {"app.accent", "#e86a9a"},
         {"main.window", "#fff0f5"},       {"main.searchFill", "#fde4ee"}, {"main.searchText", "#5b3a4a"},
         {"main.scrollbar", "#f0b6cb"},    {"card.fill", "#ffffff"},       {"card.edge", "#f9cfdf"},
         {"card.label", "#8a5a70"},        {"footer.fill", "#ffffff"},     {"footer.edge", "#f9cfdf"},
         {"footer.track", "#f2b8cd"},      {"footer.played", "#e86a9a"},   {"footer.handle", "#d94f86"},
         {"footer.speedFill", "#fde4ee"},  {"footer.speedText", "#5b3a4a"}, {"drawer.fill", "#fff7fa"},
         {"drawer.activeStart", "#f07aa8"}, {"settings.content", "#fff7fa"},
         {"settings.sidebar", "#ffffff"},  {"settings.card", "#fff0f5"},   {"settings.cardBorder", "#f9cfdf"},
         {"editor.note", "#d94f86"}},
        {{"app.surface", "#261a21"},       {"app.text", "#f5d5e3"},        {"app.accent", "#f472b6"},
         {"main.window", "#1e141a"},       {"main.searchFill", "#2f2029"}, {"card.fill", "#2c1d26"},
         {"card.edge", "#140c11"},         {"footer.fill", "#241820"},     {"footer.edge", "#140c11"},
         {"footer.track", "#5c3b4c"},      {"footer.played", "#f472b6"},   {"footer.handle", "#f9a8d4"},
         {"footer.speedFill", "#2f2029"},  {"drawer.fill", "#241820"},     {"drawer.activeStart", "#f07aa8"},
         {"popup.menuFill", "#2c1d26"},  {"settings.content", "#261a21"},
         {"settings.sidebar", "#241820"},  {"settings.card", "#2c1d26"},   {"editor.window", "#261a21"},
         {"editor.note", "#f472b6"}}));
    themes.push_back(preset(
        "builtin-ocean", "Blue",
        {{"app.surface", "#f5fbff"},       {"app.text", "#1f3b57"},        {"app.accent", "#1e88c8"},
         {"main.window", "#eef7fd"},       {"main.searchFill", "#dceefa"}, {"main.searchText", "#1f3b57"},
         {"card.fill", "#ffffff"},         {"card.edge", "#cfe6f5"},       {"footer.fill", "#ffffff"},
         {"footer.edge", "#cfe6f5"},       {"footer.track", "#a9cde6"},    {"footer.played", "#1e88c8"},
         {"footer.handle", "#0f6aa3"},     {"footer.speedFill", "#dceefa"}, {"footer.speedText", "#1f3b57"},
         {"drawer.activeStart", "#1e88c8"}, {"settings.content", "#f5fbff"},
         {"settings.sidebar", "#ffffff"},  {"settings.card", "#eef7fd"},   {"settings.cardBorder", "#cfe6f5"},
         {"editor.note", "#1e88c8"}},
        {{"app.surface", "#0f1d2b"},       {"app.text", "#cfe3f5"},        {"app.accent", "#38a3e0"},
         {"main.window", "#0b1622"},       {"main.searchFill", "#142636"}, {"card.fill", "#13253a"},
         {"card.edge", "#071019"},         {"footer.fill", "#0f2131"},     {"footer.edge", "#071019"},
         {"footer.track", "#2c4a63"},      {"footer.played", "#38a3e0"},   {"footer.handle", "#7cc7f2"},
         {"footer.speedFill", "#142636"},  {"drawer.fill", "#0f2131"},     {"drawer.activeStart", "#1e88c8"},
         {"popup.menuFill", "#13253a"},  {"settings.content", "#0f1d2b"},
         {"settings.sidebar", "#0f2131"},  {"settings.card", "#13253a"},   {"editor.window", "#0f1d2b"},
         {"editor.note", "#38a3e0"}}));
    themes.push_back(preset(
        "builtin-paper", "Black and White",
        {{"app.surface", "#ffffff"},       {"app.text", "#111111"},        {"app.accent", "#111111"},
         {"main.window", "#fafafa"},       {"main.searchFill", "#efefef"}, {"main.searchText", "#111111"},
         {"main.searchIcon", "#555555"},   {"card.fill", "#ffffff"},       {"card.edge", "#111111"},
         {"card.label", "#444444"},        {"card.author", "#111111"},     {"card.transcriber", "#111111"},
         {"card.bpm", "#111111"},          {"footer.edge", "#111111"},     {"footer.track", "#bdbdbd"},
         {"footer.played", "#111111"},     {"footer.handle", "#111111"},   {"footer.play", "#111111"},
         {"footer.skip", "#555555"},       {"footer.loop", "#111111"},     {"footer.speedFill", "#efefef"},
         {"footer.speedText", "#111111"},  {"drawer.activeStart", "#333333"}, 
         {"settings.content", "#ffffff"},  {"settings.card", "#fafafa"},   {"editor.note", "#111111"}},
        {{"app.surface", "#111111"},       {"app.text", "#eeeeee"},        {"app.accent", "#8a8a8a"},
         {"main.window", "#0a0a0a"},       {"main.searchFill", "#1c1c1c"}, {"main.searchText", "#eeeeee"},
         {"main.searchIcon", "#aaaaaa"},   {"card.fill", "#141414"},       {"card.edge", "#333333"},
         {"card.label", "#bbbbbb"},        {"card.author", "#eeeeee"},     {"card.transcriber", "#eeeeee"},
         {"card.bpm", "#eeeeee"},          {"footer.fill", "#111111"},     {"footer.edge", "#333333"},
         {"footer.track", "#555555"},      {"footer.played", "#ffffff"},   {"footer.handle", "#ffffff"},
         {"footer.play", "#ffffff"},       {"footer.skip", "#aaaaaa"},     {"footer.loop", "#eeeeee"},
         {"footer.speedFill", "#1c1c1c"},  {"footer.speedText", "#eeeeee"}, {"drawer.fill", "#111111"},
         {"drawer.activeStart", "#444444"}, {"popup.menuFill", "#141414"},
         {"settings.content", "#111111"},  {"settings.sidebar", "#0a0a0a"}, {"settings.card", "#141414"},
         {"editor.window", "#111111"},     {"editor.note", "#ffffff"}}));
    return themes;
}

}  // namespace

const std::vector<ThemeSection> &themeSections()
{
    static const std::vector<ThemeSection> sections = makeSections();
    return sections;
}

const ThemeSlot *findThemeSlot(const QString &id)
{
    for (const ThemeSection &section : themeSections()) {
        for (const ThemeSlot &entry : section.entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
    }
    return nullptr;
}

const std::vector<QuickColorSet> &quickColorSets()
{
    static const std::vector<QuickColorSet> sets = makeQuickColorSets();
    return sets;
}

const QuickColorSet &quickColorSet(PreviewScene scene)
{
    for (const QuickColorSet &set : quickColorSets()) {
        if (set.scene == scene) {
            return set;
        }
    }
    return quickColorSets().front();
}

const std::vector<QuickColor> &themeQuickColors()
{
    return quickColorSet(PreviewScene::Main).colors;
}

void applyQuickColor(ThemeColors &colors, const QuickColor &quick, const QColor &color)
{
    for (const auto &[slotId, derive] : quick.targets) {
        colors[slotId] = derive(color);
    }
}

bool isSlotChanged(const ThemeColors &colors, const ThemeColors &start, const QString &slotId)
{
    const auto now = colors.find(slotId);
    const auto before = start.find(slotId);
    if (now == colors.end() || before == start.end()) {
        return (now == colors.end()) != (before == start.end());
    }
    return now->second != before->second;
}

void restoreSlot(ThemeColors &colors, const ThemeColors &start, const QString &slotId)
{
    if (const auto before = start.find(slotId); before != start.end()) {
        colors[slotId] = before->second;
    } else {
        colors.erase(slotId);
    }
}

bool isQuickColorChanged(const ThemeColors &colors, const ThemeColors &start, const QuickColor &quick)
{
    return std::any_of(quick.targets.begin(), quick.targets.end(),
                       [&](const auto &target) { return isSlotChanged(colors, start, target.first); });
}

void restoreQuickColor(ThemeColors &colors, const ThemeColors &start, const QuickColor &quick)
{
    for (const auto &target : quick.targets) {
        restoreSlot(colors, start, target.first);
    }
}

const std::vector<ThemeDefinition> &builtInThemes()
{
    static const std::vector<ThemeDefinition> themes = makeBuiltIns();
    return themes;
}

const ThemeDefinition *findBuiltInTheme(const QString &id)
{
    for (const ThemeDefinition &theme : builtInThemes()) {
        if (theme.id == id) {
            return &theme;
        }
    }
    return nullptr;
}

QString defaultThemeId()
{
    return QString::fromLatin1(kDefaultThemeId);
}

Theme composeTheme(AppTheme base, const ThemeColors &colors)
{
    CoreColors core;
    const auto pick = [&colors](const char *id) -> std::optional<QColor> {
        const auto found = colors.find(QString::fromLatin1(id));
        return found == colors.end() ? std::nullopt : std::optional<QColor>(found->second);
    };
    core.background = pick("app.surface");
    core.fontColor = pick("app.text");
    core.hoverColor = pick("app.accent");
    Theme theme = Theme::build(base, core);
    for (const ThemeSection &section : themeSections()) {
        for (const ThemeSlot &entry : section.entries) {
            if (entry.core) {
                continue;
            }
            if (const auto found = colors.find(entry.id); found != colors.end()) {
                entry.write(theme, found->second);
            }
        }
    }
    const auto chosen = [&colors](const char *id) { return colors.count(QString::fromLatin1(id)) > 0; };
    if (!chosen("title.fill")) {
        theme.titleBarFill = theme.windowMain;
    }
    if (!chosen("title.text")) {
        theme.titleBarText = theme.fontColor;
    }
    if (!chosen("title.border")) {
        theme.windowBorder = theme.titleBarFill;
    }
    return theme;
}

Theme composeTheme(const ThemeDefinition &definition, AppTheme kind)
{
    return composeTheme(kind, definition.colors(kind));
}

QString colorToText(const QColor &color)
{
    const QString rgb = color.name(QColor::HexRgb);
    if (color.alpha() == 255) {
        return rgb;
    }
    return rgb + QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0'));
}

std::optional<QColor> colorFromText(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^#?([0-9a-fA-F]{6})([0-9a-fA-F]{2})?$"));
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    QColor color(QLatin1Char('#') + match.captured(1));
    if (match.hasCaptured(2)) {
        color.setAlpha(match.captured(2).toInt(nullptr, 16));
    }
    return color;
}

}  // namespace sky
