#pragma once
#include "application/feedback/device_feedback.hpp"
#include <QByteArray>
#include <QObject>
#include <optional>
namespace manager::adapters {
QByteArray encode_feedback(application::feedback::Event event);
std::optional<application::feedback::Event> decode_feedback(const QByteArray &frame);
class FeedbackClient final : public QObject, public application::feedback::Presenter {
  public:
    application::feedback::Delivery present(application::feedback::Event event) override;
};
} // namespace manager::adapters
