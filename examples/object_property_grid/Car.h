#ifndef CAR_H
#define CAR_H

#include <QObject>

class Car : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString brand READ brand WRITE setBrand NOTIFY brandChanged)
    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(bool electric READ electric WRITE setElectric NOTIFY electricChanged)

public:
    Car(QObject *parent = nullptr);

    QString brand() const;
    void setBrand(const QString &b);

    int year() const;
    void setYear(int y);

    bool electric() const;
    void setElectric(bool e);

signals:
    void brandChanged();
    void yearChanged();
    void electricChanged();

private:
    QString m_brand;
    bool m_electric;
    int m_year;
};

#endif // CAR_H
