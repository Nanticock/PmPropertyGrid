#include "Car.h"
#include "ObjectPropertyGrid.h"

#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Car *car1 = new Car();
    car1->setBrand("Tesla");
    car1->setYear(2025);
    car1->setElectric(true);

    Car *car2 = new Car();
    car2->setBrand("BMW");
    car2->setYear(2025);
    car2->setElectric(true);

    ObjectPropertyGrid *grid = new ObjectPropertyGrid();

    // Single object
    grid->setSelectedObject(car1);

    // Multiple objects
    grid->setSelectedObjects({car1, car2});

    grid->show();

    return app.exec();
}
