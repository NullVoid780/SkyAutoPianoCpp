#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

namespace sky {

class HoverTracker;

class EditableField final : public QWidget {
    Q_OBJECT

public:
    EditableField(const QString &label, QWidget *parent = nullptr);

    void setValue(const QString &value);
    const QString &value() const { return m_value; }
    QString editedText() const;
    void setEditable(bool editable);
    void startEditing();
    void endEditing();
    bool isEditing() const { return m_editing; }

signals:
    void editRequested();
    void commitRequested();
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refreshDisplay();

    QString m_label;
    QString m_value;
    QLabel *m_display;
    QLabel *m_labelPart;
    QLineEdit *m_edit;
    HoverTracker *m_hover;
    bool m_editable = true;
    bool m_editing = false;
};

}  // namespace sky
