#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QTreeWidgetItem>
#include <QVBoxLayout>

static void addWidgetToTree(QTreeWidgetItem *parentItem, QWidget *widget)
{
    if (!widget)
        return;

    auto *item = new QTreeWidgetItem(parentItem, {widget->objectName()});
    item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<QObject *>(widget)));

    const QList<QWidget *> children = widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children)
        addWidgetToTree(item, child);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), propertyGrid(this)
{
    ui->setupUi(this);

    // Place propertyGrid inside the Properties dock
    propertyGrid.layout()->setContentsMargins(0, 0, 0, 0);
    ui->dockWidget_2->setWidget(&propertyGrid);

    // Populate tree with central widget hierarchy
    ui->treeWidget->setHeaderLabel("Objects");
    ui->treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Instead of adding the central widget itself, add its direct children
    const QList<QWidget *> children = ui->centralwidget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children)
        addWidgetToTree(ui->treeWidget->invisibleRootItem(), child);

    // Expand everything by default
    ui->treeWidget->expandAll();

    // Default selection: first item
    if (ui->treeWidget->topLevelItemCount() > 0)
    {
        ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(0));
        QObject *obj = ui->treeWidget->topLevelItem(0)->data(0, Qt::UserRole).value<QObject *>();

        if (obj)
            propertyGrid.setSelectedObject(obj);
    }

    // Sync selection with property grid
    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::objectsTreeSelectionChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::objectsTreeSelectionChanged()
{
    QList<QObject *> selectedObjects;
    for (QTreeWidgetItem *item : ui->treeWidget->selectedItems())
    {
        QObject *obj = item->data(0, Qt::UserRole).value<QObject *>();
        if (obj)
            selectedObjects.append(obj);
    }

    if (selectedObjects.isEmpty())
        propertyGrid.setSelectedObjects({});
    else if (selectedObjects.size() == 1)
        propertyGrid.setSelectedObject(selectedObjects.first());
    else
        propertyGrid.setSelectedObjects(selectedObjects);
}
