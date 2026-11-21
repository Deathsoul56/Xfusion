#pragma once

#include <QFormLayout>
#include <QWidget>
#include <algorithm>

// Gives the labels of every form under `page` one width, so that the boxes of different groups start at the same
// distance from the edge.
inline void alignFormLabels(QWidget* page)
{
    int width = 0;
    const auto forms = page->findChildren<QFormLayout*>();
    for (const QFormLayout* form : forms)
        for (int row = 0; row < form->rowCount(); ++row)
            if (const QLayoutItem* item = form->itemAt(row, QFormLayout::LabelRole))
                if (item->widget())
                    width = std::max(width, item->widget()->sizeHint().width());
    for (const QFormLayout* form : forms)
        for (int row = 0; row < form->rowCount(); ++row)
            if (const QLayoutItem* item = form->itemAt(row, QFormLayout::LabelRole))
                if (item->widget())
                    item->widget()->setMinimumWidth(width);
}
