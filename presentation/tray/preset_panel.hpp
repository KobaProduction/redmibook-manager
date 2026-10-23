#pragma once
#include "application/presets/model.hpp"
#include <QVBoxLayout>
#include <QWidget>
class QPushButton;

namespace manager::presentation {
class PresetPanel final : public QWidget {
    Q_OBJECT
  public:
    explicit PresetPanel(QWidget *parent = nullptr);
    void updatePresets(const std::vector<application::presets::Preset> &presets, bool pending);
  signals:
    void requested(const QString &preset, const QString &button);

  protected:
    void changeEvent(QEvent *event) override;

  private:
    QVBoxLayout layout_{this};
    std::vector<application::presets::Preset> presets_;
    void rebuild();
    void appendPreset(const application::presets::Preset &preset);
    QPushButton *makeButton(const application::presets::Preset &preset,
                            const application::presets::Button &config);
};
} // namespace manager::presentation
