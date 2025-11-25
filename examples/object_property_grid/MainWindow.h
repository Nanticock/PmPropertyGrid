#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ObjectPropertyGrid.h"

#include <QMainWindow>

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void objectsTreeSelectionChanged();

private:
    Ui::MainWindow *ui;

    ObjectPropertyGrid propertyGrid;
};

#endif // MAINWINDOW_H
