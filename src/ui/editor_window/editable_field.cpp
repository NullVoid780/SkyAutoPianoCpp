#include "ui/editor_window/editable_field.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPadding = 4;
constexpr qreal kRadius = 4;
constexpr int kLabelGap = 4;
constexpr int kMinEditWidth = 30;

}  // namespace

EditableField::EditableField(const QString &label, QWidget *parent)
    : QWidget(parent),
      m_label(label),
      m_display(new QLabel(this)),
      m_labelPart(new QLabel(label, this)),
      m_edit(new QLineEdit(this)),
      m_hover(new HoverTracker(this, kHoverMs, Easing::Ease))
{
    setCursor(Qt::PointingHandCursor);
    m_display->setObjectName(QStringLiteral("EditorFieldText"));
    m_display->setTextFormat(Qt::PlainText);
    m_display->setWordWrap(true);
    m_display->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_labelPart->setObjectName(QStringLiteral("EditorFieldText"));
    m_labelPart->setVisible(false);
    m_edit->setObjectName(QStringLiteral("EditorFieldEdit"));
    m_edit->setMinimumWidth(kMinEditWidth);
    m_edit->setVisible(false);
    m_edit->installEventFilter(this);

    auto *editRow = new QHBoxLayout();
    editRow->setContentsMargins(0, 0, 0, 0);
    editRow->setSpacing(kLabelGap);
    editRow->addWidget(m_labelPart);
    editRow->addWidget(m_edit, 1);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addWidget(m_display);
    column->addLayout(editRow);

    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(m_edit, &QLineEdit::returnPressed, this, &EditableField::commitRequested);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void EditableField::setValue(const QString &value)
{
    m_value = value;
    refreshDisplay();
}

QString EditableField::editedText() const
{
    return m_edit->text();
}

void EditableField::setEditable(bool editable)
{
    m_editable = editable;
    setCursor(editable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void EditableField::refreshDisplay()
{
    m_display->setText(m_label.isEmpty() ? m_value : m_label + QLatin1Char(' ') + m_value);
}

void EditableField::startEditing()
{
    m_editing = true;
    m_display->setVisible(false);
    m_labelPart->setVisible(!m_label.isEmpty());
    m_edit->setVisible(true);
    m_edit->setText(m_value);
    m_edit->selectAll();
    m_edit->setFocus(Qt::OtherFocusReason);
    update();
}

void EditableField::endEditing()
{
    m_editing = false;
    m_edit->setVisible(false);
    m_labelPart->setVisible(false);
    m_display->setVisible(true);
    update();
}

void EditableField::mousePressEvent(QMouseEvent *event)
{
    if (m_editable && !m_editing) {
        emit editRequested();
    }
    QWidget::mousePressEvent(event);
}

bool EditableField::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        emit cancelRequested();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void EditableField::paintEvent(QPaintEvent *)
{
    const EditorPalette &e = currentTheme().editor;
    QPainter painter(this);
    preparePainter(painter);
    QColor fill = m_editing ? e.rowEditFill : withAlpha(e.rowHoverFill, m_editable ? m_hover->progress() : 0.0);
    painter.fillPath(roundedPath(QRectF(rect()), kRadius), fill);
}

}  // namespace sky
