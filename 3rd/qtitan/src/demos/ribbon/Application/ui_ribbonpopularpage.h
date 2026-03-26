/********************************************************************************
** Form generated from reading UI file 'ribbonpopularpage.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RIBBONPOPULARPAGE_H
#define UI_RIBBONPOPULARPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_pageCustomizePopular
{
public:
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    QLabel *labelTitle;
    QFrame *frame;
    QLabel *labelTheme;
    QComboBox *comboBoxTheme;
    QLabel *labelAccentColor;
    QPushButton *pushButtonAccentColor;
    QLabel *labelBackgroundImage;
    QComboBox *comboBoxBackground;
    QFrame *frameAccentColor;

    void setupUi(QWidget *customizePopularPage)
    {
        if (customizePopularPage->objectName().isEmpty())
            customizePopularPage->setObjectName(QString::fromUtf8("customizePopularPage"));
        customizePopularPage->resize(600, 380);
        customizePopularPage->setMinimumSize(QSize(600, 380));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/res/popularpage.png"), QSize(), QIcon::Normal, QIcon::Off);
        customizePopularPage->setWindowIcon(icon);
        horizontalLayout = new QHBoxLayout(customizePopularPage);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        verticalLayout->setContentsMargins(-1, 0, -1, 0);
        labelTitle = new QLabel(customizePopularPage);
        labelTitle->setObjectName(QString::fromUtf8("labelTitle"));
        labelTitle->setFrameShape(QFrame::StyledPanel);
        labelTitle->setFrameShadow(QFrame::Plain);
        labelTitle->setMargin(4);

        verticalLayout->addWidget(labelTitle);

        frame = new QFrame(customizePopularPage);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        labelTheme = new QLabel(frame);
        labelTheme->setObjectName(QString::fromUtf8("labelTheme"));
        labelTheme->setGeometry(QRect(40, 30, 81, 16));
        comboBoxTheme = new QComboBox(frame);
        comboBoxTheme->setObjectName(QString::fromUtf8("comboBoxTheme"));
        comboBoxTheme->setGeometry(QRect(150, 28, 219, 23));
        labelAccentColor = new QLabel(frame);
        labelAccentColor->setObjectName(QString::fromUtf8("labelAccentColor"));
        labelAccentColor->setGeometry(QRect(40, 63, 171, 16));
        pushButtonAccentColor = new QPushButton(frame);
        pushButtonAccentColor->setObjectName(QString::fromUtf8("pushButtonAccentColor"));
        pushButtonAccentColor->setGeometry(QRect(353, 61, 17, 23));
        labelBackgroundImage = new QLabel(frame);
        labelBackgroundImage->setObjectName(QString::fromUtf8("labelBackgroundImage"));
        labelBackgroundImage->setGeometry(QRect(40, 98, 191, 16));
        comboBoxBackground = new QComboBox(frame);
        comboBoxBackground->setObjectName(QString::fromUtf8("comboBoxBackground"));
        comboBoxBackground->setGeometry(QRect(246, 96, 124, 23));
        frameAccentColor = new QFrame(frame);
        frameAccentColor->setObjectName(QString::fromUtf8("frameAccentColor"));
        frameAccentColor->setGeometry(QRect(246, 61, 105, 23));
        frameAccentColor->setFrameShape(QFrame::StyledPanel);
        frameAccentColor->setFrameShadow(QFrame::Plain);

        verticalLayout->addWidget(frame);

        verticalLayout->setStretch(1, 1);

        horizontalLayout->addLayout(verticalLayout);

#ifndef QT_NO_SHORTCUT
        labelTheme->setBuddy(comboBoxTheme);
        labelAccentColor->setBuddy(pushButtonAccentColor);
        labelBackgroundImage->setBuddy(comboBoxBackground);
#endif // QT_NO_SHORTCUT

        retranslateUi(customizePopularPage);

        QMetaObject::connectSlotsByName(customizePopularPage);
    } // setupUi

    void retranslateUi(QWidget *customizePopularPage)
    {
        customizePopularPage->setWindowTitle(QApplication::translate("pageCustomizePopular", "Popular", nullptr));
#ifndef QT_NO_STATUSTIP
        customizePopularPage->setStatusTip(QApplication::translate("pageCustomizePopular", "Change the most popular options", nullptr));
#endif // QT_NO_STATUSTIP
        labelTitle->setText(QApplication::translate("pageCustomizePopular", "<html><head/><body><p><span style=\" font-weight:600;\">Top options for Working with Demo</span></p></body></html>", nullptr));
        labelTheme->setText(QApplication::translate("pageCustomizePopular", "&Theme:", nullptr));
        labelAccentColor->setText(QApplication::translate("pageCustomizePopular", "Accen&t Color (Office 2013 only):", nullptr));
        pushButtonAccentColor->setText(QString());
        labelBackgroundImage->setText(QApplication::translate("pageCustomizePopular", "&Background Image (Office 2013 only):", nullptr));
    } // retranslateUi

};

namespace Ui {
    class pageCustomizePopular: public Ui_pageCustomizePopular {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RIBBONPOPULARPAGE_H
