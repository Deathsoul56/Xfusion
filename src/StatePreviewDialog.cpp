#include "StatePreviewDialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "StateFile.h"

namespace {

constexpr QSize kThumbnailSize(128, 112);
constexpr int kBanks = 10;

}

StatePreviewDialog::StatePreviewDialog(const GameSaves& saves, bool saving, int bank, QWidget* parent)
    : QDialog(parent)
    , m_saves(saves)
    , m_bank(new QComboBox(this))
{
    setWindowTitle(saving ? "Save with Preview" : "Load with Preview");

    for (int i = 0; i < kBanks; ++i)
        m_bank->addItem(QString("Bank #%1").arg(i));
    m_bank->setCurrentIndex(qBound(0, bank, kBanks - 1));
    connect(m_bank, &QComboBox::currentIndexChanged, this, &StatePreviewDialog::refresh);

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel("Bank:", this));
    top->addWidget(m_bank);
    top->addStretch(1);

    auto* grid = new QGridLayout;
    for (int i = 0; i < 10; ++i) {
        auto* button = new QPushButton(this);
        button->setIconSize(kThumbnailSize);
        button->setMinimumSize(kThumbnailSize.width() + 24, kThumbnailSize.height() + 56);
        connect(button, &QPushButton::clicked, this, [this, i] { emit slotChosen(m_bank->currentIndex() * 10 + i); });
        m_buttons[static_cast<std::size_t>(i)] = button;
        grid->addWidget(button, i / 5, i % 5);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(grid);
    layout->addWidget(buttons);
    refresh();
}

void StatePreviewDialog::refresh()
{
    for (int i = 0; i < 10; ++i) {
        const int slot = m_bank->currentIndex() * 10 + i;
        QPushButton* button = m_buttons[static_cast<std::size_t>(i)];
        QImage thumbnail;
        const auto header = StateFile::readHeader(m_saves.statePath(slot), &thumbnail);
        const QString name = QString("Slot #%1").arg(i);
        if (!header) {
            button->setText(name + "\nempty");
            button->setIcon({});
            continue;
        }
        button->setText(name + "\n" + header->created.toLocalTime().toString("yyyy-MM-dd HH:mm"));
        if (thumbnail.isNull())
            button->setIcon({});
        else
            button->setIcon(QPixmap::fromImage(thumbnail.scaled(kThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
}
