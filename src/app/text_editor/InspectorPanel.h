#pragma once

#include <QFrame>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QScrollArea;
class QTabWidget;
class QVBoxLayout;
class QWidget;

namespace TherionStudio
{

class InspectorPanel : public QFrame
{
    Q_OBJECT

public:
    explicit InspectorPanel(QWidget *parent = nullptr);

    QTabWidget *tabs() const;
    QFrame *leftEdge() const;

    QWidget *addPlainTab(const QString &title);
    QWidget *addScrollTab(const QString &title, const QString &objectName = QString());

    static QFrame *createSection(QWidget *parent,
                                 const QString &title,
                                 QVBoxLayout **contentLayout,
                                 QLabel **titleLabelOut = nullptr);

    void updateLeftEdgeGeometry();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static void configureScrollArea(QScrollArea *scrollArea);

    void rebuildSelector();
    void syncSelectorFromTabs();

    QWidget *selector_ = nullptr;
    QHBoxLayout *selectorLayout_ = nullptr;
    QButtonGroup *selectorButtons_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QFrame *leftEdge_ = nullptr;
};

}
