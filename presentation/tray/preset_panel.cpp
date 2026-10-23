#include "presentation/tray/preset_panel.hpp"
#include "presentation/common/icons.hpp"
#include "presentation/common/preset_titles.hpp"
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStyleOptionButton>
#include <algorithm>

namespace manager::presentation {
namespace {
class PresetButton final : public QPushButton {
  public:
    using QPushButton::QPushButton;

  protected:
    void paintEvent(QPaintEvent *) override {
        QStyleOptionButton option;
        initStyleOption(&option);
        const int icon = option.icon.isNull() ? 0 : iconSize().width() + 6;
        option.text = fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, width() - icon - 20));
        QPainter painter(this);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
    }
};
void clear_layout(QLayout &layout) {
    while (auto *item = layout.takeAt(0)) {
        if (auto *child = item->layout())
            clear_layout(*child);
        delete item->widget();
        delete item;
    }
}
QLabel *make_title(const QString &title) {
    auto *label = new QLabel;
    label->setTextFormat(Qt::PlainText);
    label->setText(label->fontMetrics().elidedText(title, Qt::ElideRight, 360));
    label->setToolTip(title);
    return label;
}
} // namespace
PresetPanel::PresetPanel(QWidget *parent) : QWidget(parent) {
    layout_.setContentsMargins(0, 0, 0, 0);
    layout_.setSpacing(8);
    setVisible(false);
}
void PresetPanel::updatePresets(const std::vector<application::presets::Preset> &presets, bool pending) {
    if (presets_ != presets) {
        presets_ = presets;
        rebuild();
    }
    setEnabled(!pending);
    setVisible(std::ranges::any_of(presets_, [](const auto &preset) { return preset.visible; }));
}
void PresetPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::LanguageChange)
        rebuild();
    QWidget::changeEvent(event);
}
void PresetPanel::rebuild() {
    clear_layout(layout_);
    for (const auto &preset : presets_)
        if (preset.visible)
            appendPreset(preset);
}
void PresetPanel::appendPreset(const application::presets::Preset &preset) {
    if (preset.buttons.empty())
        return;
    layout_.addWidget(make_title(preset_title(preset.title, preset.default_title)));
    auto *row = new QGridLayout;
    row->setSpacing(4);
    const int columns = static_cast<int>(std::min<std::size_t>(4, preset.buttons.size()));
    for (std::size_t index = 0; index < preset.buttons.size(); ++index)
        row->addWidget(makeButton(preset, preset.buttons[index]), static_cast<int>(index) / columns,
                       static_cast<int>(index) % columns);
    for (int column = 0; column < columns; ++column)
        row->setColumnStretch(column, 1);
    layout_.addLayout(row);
}
QPushButton *PresetPanel::makeButton(const application::presets::Preset &preset,
                                     const application::presets::Button &config) {
    auto *button = new PresetButton(preset_title(config.title, config.default_title));
    button->setObjectName(QString::fromStdString(config.id));
    button->setMinimumWidth(0);
    button->setMinimumHeight(30);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    button->setToolTip(button->text());
    button->setAccessibleName(button->text());
    button->setIcon(QIcon(symbol_pixmap(QString::fromStdString(config.icon),
                                        palette().color(QPalette::WindowText), 16, devicePixelRatioF())));
    button->setIconSize(QSize(16, 16));
    connect(button, &QPushButton::clicked, this,
            [this, id = QString::fromStdString(preset.id), key = QString::fromStdString(config.id)] {
                emit requested(id, key);
            });
    return button;
}
} // namespace manager::presentation
