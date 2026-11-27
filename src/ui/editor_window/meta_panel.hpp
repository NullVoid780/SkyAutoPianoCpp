#pragma once

#include <array>

#include <QWidget>

namespace sky {

class EditableField;

class MetaPanel final : public QWidget {
    Q_OBJECT

public:
    enum Field { Name, Author, TranscribedBy, Bpm };

    explicit MetaPanel(QWidget *parent = nullptr);

    EditableField *field(Field field) const { return m_fields[static_cast<std::size_t>(field)]; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::array<EditableField *, 4> m_fields{};
};

}  // namespace sky
