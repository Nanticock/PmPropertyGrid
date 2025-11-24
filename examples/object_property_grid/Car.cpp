#include "Car.h"

Car::Car(QObject *parent) : QObject(parent), m_electric(false), m_year(2025)
{
}

QString Car::brand() const
{
    return m_brand;
}

void Car::setBrand(const QString &b)
{
    if (m_brand == b)
        return;

    m_brand = b;
    emit brandChanged();
}

int Car::year() const
{
    return m_year;
}

void Car::setYear(int y)
{
    if (m_year == y)
        return;

    m_year = y;
    emit yearChanged();
}

bool Car::electric() const
{
    return m_electric;
}

void Car::setElectric(bool e)
{
    if (m_electric != e)
        return;

    m_electric = e;
    emit electricChanged();
}
