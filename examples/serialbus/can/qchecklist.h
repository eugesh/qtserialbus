/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QCHECKLIST
#define QCHECKLIST

#include <QWidget>
#include <QComboBox>
#include <QStandardItemModel>
#include <QLineEdit>
#include <QEvent>
#include <QStyledItemDelegate>
#include <QListView>

/**
 * @brief QComboBox with support of checkboxes
 * http://stackoverflow.com/questions/8422760/combobox-of-checkboxes
 *
 * @author Damien Sorel <damien@sorel.me>
 */
class QCheckList : public QComboBox
{
    Q_OBJECT

public:
    /**
     * @brief Additional value to Qt::CheckState when some checkboxes are Qt::PartiallyChecked
     */
    static const int StateUnknown = 3;

private:
    QStandardItemModel* m_model;
    /**
     * @brief Text displayed when no item is checked
     */
    QString m_noneCheckedText;
    /**
     * @brief Text displayed when all items are checked
     */
    QString m_allCheckedText;
    /**
     * @brief Text displayed when some items are partially checked
     */
    QString m_unknownlyCheckedText;

signals:
    void globalCheckStateChanged(int);
    void checkStateChanged(int index, Qt::CheckState);

public:
    QCheckList(QWidget* _parent = nullptr) : QComboBox(_parent)
    {
        m_model = new QStandardItemModel();
        setModel(m_model);

        setEditable(true);
        lineEdit()->setReadOnly(true);
        lineEdit()->installEventFilter(this);
        setItemDelegate(new QCheckListStyledItemDelegate(this));

        connect(lineEdit(), &QLineEdit::selectionChanged, lineEdit(), &QLineEdit::deselect);
        connect(view(), &QAbstractItemView::pressed, this, &QCheckList::on_itemPressed);
        connect(m_model, &QAbstractItemModel::dataChanged, this, &QCheckList::on_modelDataChanged);
    }

    ~QCheckList()
    {
        delete m_model;
    }

    void setAllCheckedText(const QString &text)
    {
        m_allCheckedText = text;
        updateText();
    }

    void setNoneCheckedText(const QString &text)
    {
        m_noneCheckedText = text;
        updateText();
    }

    void setUnknownlyCheckedText(const QString &text)
    {
        m_unknownlyCheckedText = text;
        updateText();
    }

    /**
     * @brief Adds a item to the checklist (setChecklist must have been called)
     * @return the new QStandardItem
     */
    QStandardItem* addCheckItem(const QString &label, const QVariant &data, const Qt::CheckState checkState)
    {
        QStandardItem* item = new QStandardItem(label);
        // item->setCheckable(true);
        item->setCheckState(checkState);
        item->setData(data);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);

        m_model->appendRow(item);

        updateText();

        return item;
    }

    QVector<QStandardItem*> checkedItems()
    {
        QVector<QStandardItem*> items;

        for (int i = 0; i < m_model->rowCount(); i++)
        {
            if (m_model->item(i)->checkState() == Qt::Checked)
                items.append(m_model->item(i));
        }

        return items;
    }

    bool isChecked(int index) const
    {
        return (m_model->item(index)->checkState() == Qt::Checked);
    }

    bool isChecked(const QString &text) const
    {
        return (m_model->item(findText(text))->checkState() == Qt::Checked);
    }

    void setChecked(int index, Qt::CheckState checked = Qt::Checked)
    {
        m_model->item(index)->setCheckState(checked);
    }

    void setChecked(const QString &text, Qt::CheckState checked = Qt::Checked)
    {
        m_model->item(findText(text))->setCheckState(checked);
    }

    /**
     * @brief Computes the global state of the checklist :
     *      - if there is no item: StateUnknown
     *      - if there is at least one item partially checked: StateUnknown
     *      - if all items are checked: Qt::Checked
     *      - if no item is checked: Qt::Unchecked
     *      - else: Qt::PartiallyChecked
     */
    int globalCheckState()
    {
        int nbRows = m_model->rowCount(), nbChecked = 0, nbUnchecked = 0;

        if (nbRows == 0)
        {
            return StateUnknown;
        }

        for (int i = 0; i < nbRows; i++)
        {
            if (m_model->item(i)->checkState() == Qt::Checked)
            {
                nbChecked++;
            }
            else if (m_model->item(i)->checkState() == Qt::Unchecked)
            {
                nbUnchecked++;
            }
            else
            {
                return StateUnknown;
            }
        }

        return nbChecked == nbRows ? Qt::Checked : nbUnchecked == nbRows ? Qt::Unchecked : Qt::PartiallyChecked;
    }

protected:
    bool eventFilter(QObject* _object, QEvent* _event)
    {
        if (_object == lineEdit() && _event->type() == QEvent::MouseButtonPress)
        {
            showPopup();
            return true;
        }

        return false;
    }

private:
    void updateText()
    {
        QString text;

        switch (globalCheckState())
        {
        case Qt::Checked:
            text = m_allCheckedText;
            break;

        case Qt::Unchecked:
            text = m_noneCheckedText;
            break;

        case Qt::PartiallyChecked:
            for (int i = 0; i < m_model->rowCount(); i++)
            {
                if (m_model->item(i)->checkState() == Qt::Checked)
                {
                    if (!text.isEmpty())
                    {
                        text+= ", ";
                    }

                    text+= m_model->item(i)->text();
                }
            }
            break;

        default:
            text = m_unknownlyCheckedText;
        }

        lineEdit()->setText(text);
    }

private slots:
    void on_modelDataChanged()
    {
        updateText();
        emit globalCheckStateChanged(globalCheckState());
    }

    void on_itemPressed(const QModelIndex &index)
    {
        QStandardItem* item = m_model->itemFromIndex(index);

        if (item->checkState() == Qt::Checked)
        {
            item->setCheckState(Qt::Unchecked);
        }
        else
        {
            item->setCheckState(Qt::Checked);
        }

        emit checkStateChanged(index.row(), item->checkState());
    }

public:
    class QCheckListStyledItemDelegate : public QStyledItemDelegate
    {
    public:
        QCheckListStyledItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

        void paint(QPainter * painter_, const QStyleOptionViewItem & option_, const QModelIndex & index_) const
        {
            QStyleOptionViewItem & refToNonConstOption = const_cast<QStyleOptionViewItem &>(option_);
            refToNonConstOption.showDecorationSelected = false;
            QStyledItemDelegate::paint(painter_, refToNonConstOption, index_);
        }
    };
};

#endif // QCHECKLIST
