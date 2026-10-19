#include "presentation/about_model.hpp"
#include <QFile>
namespace manager::presentation {
QString AboutModel::profileStatus() const {
    if (demo()) {
        return tr("Sample data, not detected hardware.");
    }
    switch (profile_.status) {
    case application::ProfileReadStatus::Available:
        return tr("Reported by SMBIOS. Hardware controls require separate compatibility verification.");
    case application::ProfileReadStatus::NotImplemented:
        return tr("Device identification is not implemented on this platform.");
    case application::ProfileReadStatus::Failed:
        return tr("Device identification failed. Hardware controls remain unavailable.");
    }
    return {};
}
QString AboutModel::mitTranslation() const {
    return tr(R"MIT(MIT License

Copyright (c) 2026 ArthurKoba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.)MIT");
}

QString AboutModel::licenseText(int index, bool original) const {
    if (index == 0 && !original) {
        return mitTranslation();
    }
    const QString path = index == 0   ? QStringLiteral(":/licenses/LICENSE")
                         : index == 1 ? QStringLiteral(":/licenses/packaging/licenses/Qt/LGPL-3.0-only.txt")
                                      : QStringLiteral(":/licenses/packaging/THIRD_PARTY_NOTICES.md");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return tr("License text is unavailable.");
    }
    auto text = QString::fromUtf8(file.readAll());
    if (index == 1) {
        QFile gpl(QStringLiteral(":/licenses/packaging/licenses/Qt/GPL-3.0-only.txt"));
        if (!gpl.open(QIODevice::ReadOnly)) {
            return tr("License text is unavailable.");
        }
        text += QStringLiteral("\n\n") + QString::fromUtf8(gpl.readAll());
    }
    return text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
}
} // namespace manager::presentation
