#include "airplane.h"
#include <QPainter>
#include <QStyleOption>
#include <cmath>
#include <QRectF>

int Airplane::nOfPlanes = 0;

QVector<AirplaneData> Airplane::data {};

static double normalizeAngle(double angle)
{
    while (angle < 0) {
        angle += 360;
    }

    while (angle > 360) {
        angle -= 360;
    }

    return angle;
}

double Airplane::calculateAngle()
{
    QLineF newDir(mapFromScene(direction), mapFromScene(0, 0));
    double angle = std::acos(newDir.dx() / newDir.length());
    if(newDir.dy() < 0)
        angle = 2 * M_PI - angle;
    angle = qRadiansToDegrees(angle);
    angle += 90;
    angle = normalizeAngle(angle);
    if(angle > 180)
        angle -= 360;
    return angle;
}

Airplane::Airplane(QPointF pos, const QPointF target, int t)
    : type(t == -1 ? nOfPlanes % data.size() : t),
      image(data[type].image),
      speed(data[type].speed),
      maxAngle(data[type].maxAngle),
      fuelCap(data[type].fuelCap),
      fuelUse(data[type].fuelUse),
      origin(std::move(pos)),
      target(std::move(target))
{
    setPos(pos);

    wastedFuel = 0;
    steerLeft = true;
    stillDangerous = false;

    // Plane is flying from pos to target, and spawns with some fuel
//    setOrigin(pos);
//    setTarget(target);
    fuel = fuelCap;
    state = State::FLYING;

    flightNo = nOfPlanes++;

    // Calculate direction vector of the plane
    direction = pos - target;
    double toTarget = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
//    qDebug() << toTarget;
    direction /= toTarget;

    currentAngle = calculateAngle();
    setRotation(currentAngle);

    // Call update() every 50 miliseconds
    static QTimer timer1;
    connect(&timer1, SIGNAL(timeout()), this, SLOT(update()));
    timer1.start(50);

    // Move the plane ever 50 miliseconds
    //TODO static timer instead of member
    static QTimer timer2;
    connect(&timer2, SIGNAL(timeout()), this, SLOT(move()));
    timer2.start(50);

//    qDebug() << "Flight-" + QString::number(flightNo) + " taking off";
//    qDebug() << "Spawned a plane at " << pos;

}

Airplane::~Airplane()
{
//    qDebug() << "Flight-" + QString::number(flightNo) + " just State::CRASHED";
}

QRectF Airplane::boundingRect() const
{
    qreal adjust = -0.5;
    return QRectF(-36 - adjust, -44 - adjust,
                  72 + adjust, 120 + adjust);
}

QPainterPath Airplane::shape() const
{
    QPainterPath path;
    qreal adjust = -0.5;
    QRectF rect(-9 - adjust, -11 - adjust,
                  18 + adjust, 30 + adjust);

    path.addRect(rect);
    return path;
}

void Airplane::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    // if it is far away, the plane is drawn just as a circle
    double detail = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    if(detail < 0.6) {
        painter->setBrush(Qt::red);
        painter->drawEllipse(0, 0, 20, 20);
    } else {
        QPixmap img(image);
    //    painter->setCompositionMode(QPainter::CompositionMode_SourceIn);
    //    painter->fillRect(img.rect(), Qt::red);
        painter->drawPixmap(-20, -20, 40, 50, img);

        painter->drawRect(-15, 30, 30, 5);
        if(state == State::DANGER) painter->setBrush(Qt::red);
        else if (state == State::FLYING) painter->setBrush(Qt::green);
        else if (state == State::HOLDING) painter->setBrush(Qt::yellow);
        else if (state == State::MANUAL) painter->setBrush(Qt::blue);
        else painter->setBrush(Qt::black);
        painter->drawEllipse(20, 30, 10, 10);

        double fuelRatio = fuel / fuelCap;
        int r = static_cast<int>(255.0 * (1 - fuelRatio));
        int g = static_cast<int>(255.0 * fuelRatio);
        int b = 0;
        painter->setBrush(QColor(r, g, b));
        painter->drawRect(-15, 30, static_cast<int>(30 * fuelRatio), 5);
    }

}

State Airplane::getState()
{
    return state;
}

void Airplane::setState(State state)
{
    if(this->state != State::CRASHED){
        this->state = state;
    }
}

double Airplane::getDistance()
{
    QPointF d = pos() - target;
    return qSqrt(d.x() * d.x() + d.y() * d.y());
}

double Airplane::getFuel()
{
    return fuel;
}

double Airplane::getWastedFuel()
{
    return wastedFuel;
}

double Airplane::calcFuel(QPointF o, QPointF t)
{
    QPointF d = o - t;
    return qSqrt(d.x() * d.x() + d.y() * d.y()) / speed;
}

void Airplane::move(){

    if(state == State::CRASHED) return;

    // Move the plane forward
    setPos(mapToParent(0, -speed));
    setRotation(rotation() + currentAngle);
    setScale(0.5);
//    setPos(pos() - speed * direction);
    fuel -= fuelUse;

    if(state == State::MANUAL || state == State::DANGER){
        currentAngle = 0;
        return;
    }else if(state == State::FLYING){
        moveToTarget();
    }else if(state == State::HOLDING){
        holdingPattern();
    }


}

void Airplane::update()
{
    if(state == State::CRASHED) {
        return;
    }

    // Steer if there are planes dangerously close
    bool inDanger = false;

    const auto dangerPlanes = scene()->items(
                QPolygonF(
                    { mapToScene(0, 0),
                      mapToScene(-50, 0),
                      mapToScene(-50, -80),
                      mapToScene(50, -80),
                      mapToScene(50, 0)}
                ), Qt::IntersectsItemShape);

    for (auto item: dangerPlanes) {

        if(state == State::MANUAL) {
            inDanger = false;
            break;
        }

        if(item == this) continue;
        Airplane* plane = dynamic_cast<Airplane*>(item);

        if(plane && plane->type == type){
            inDanger = true;

            QLineF lineToPlane(QPointF(0, 0), mapFromItem(item, 0, 0));

            qreal angleToPlane = acos(lineToPlane.dx() / lineToPlane.length());
            if(lineToPlane.dy() < 0) angleToPlane = M_PI * 2 - angleToPlane;

            angleToPlane = normalizeAngle((/*180 -  */qRadiansToDegrees(angleToPlane)) + 90);

            angleToPlane = qDegreesToRadians(angleToPlane);

            if(angleToPlane >= M_PI) {
                // Rotate left
               steer(angleToPlane > maxAngle ? maxAngle : angleToPlane);
                steerLeft = false;
            } else if(angleToPlane <= M_PI) {
                // Rotate right
                steer(angleToPlane > maxAngle ? -maxAngle : -angleToPlane);
                steerLeft = true;
            }
        }
    }
    if(inDanger) {
//        qDebug() << "in danger" << this->flightNo;
        state = State::DANGER;
    } else {
        if(state == State::DANGER) {
//            qDebug() << "not in danger any more";
            state = State::FLYING;
            stillDangerous = true;
        }
    }

    // Check if the plane collided with other planes and if so, destroy all planes that collided
    QList<QGraphicsItem*> crashedPlanes = collidingItems(Qt::IntersectsItemShape);
    foreach(QGraphicsItem* item, crashedPlanes){

        Airplane* plane = dynamic_cast<Airplane*>(item);

        if(plane && plane->state != State::CRASHED && plane->type == type){

            QString s = "Flight-" + QString::number(flightNo) + " crashed with "
                    + "Flight-" + QString::number(plane->flightNo);
            emit finished(s, true);

            plane->setState(State::CRASHED);
            state = State::CRASHED;
            plane->deleteLater();

        }
    }

    // If the fuel reaches critical point, nnotify the user
    static bool low = false;
    if(fuel <= fuelCap / 5 && !low){
        low = true;
        QString s = "Flight-" + QString::number(flightNo) + " is low on fuel";
        emit finished(s, true);
    }

    // If the fuel runs out, the plane crashes
    if(fuel <= 0){
        state = State::CRASHED;
        QString s = "Flight-" + QString::number(flightNo) + " ran out of fuel";
        emit finished(s, true);
//        return;
    }

    if(state == State::CRASHED) {
        deleteLater();
    }
}

void Airplane::moveToTarget(){

    if(stillDangerous) {
        holdingPattern();
        stillDangerous = false;
        return;
    }
    // If plane arrived at target, land and refuel
    QPointF d = pos() - target;
    double toTarget = qSqrt(d.x() * d.x() + d.y() * d.y());

    if(toTarget <= 15) {
//        qDebug() << "aaa";
        land();
    }

    // Determine if the plane should steer left or right
    double a = direction.y();
    double b = -direction.x();
    double c = direction.x() * y() - direction.y() * x();
    double f = a * target.x() + b * target.y() + c;

    double dot = d.x() * direction.x() + d.y() * direction.y();
    double d_norm = toTarget;
    double dir_norm = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());

    double pam = dot / (d_norm * dir_norm);
    if(pam > 1) pam = 1;
    if(pam < -1) pam = -1;
    double angle = acos(pam);
    if(angle > maxAngle) angle = maxAngle;
    if(f > 0) {
        steer(angle);
    } else {
        steer(-angle);
    }

}


void Airplane::holdingPattern(){
    wastedFuel += fuelUse;
    if(steerLeft) {
        steer(-maxAngle);
    } else {
        steer(maxAngle);
    }
}

void Airplane::land(){

    QString s = "Flight-" + QString::number(flightNo) + " just landed";
    emit finished(s, false);
    deleteLater();
}

void Airplane::steer(double theta)
{
    double x = qCos(theta) * direction.x() - qSin(theta) * direction.y();
    double y = qSin(theta) * direction.x() + qCos(theta) * direction.y();

    direction.setX(x);
    direction.setY(y);

    // Angle between previous and current direction vector
    currentAngle = calculateAngle();
}

void Airplane::setTarget(const QPointF target)
{
    this->target = target;
}

QPointF Airplane::getTarget()
{
    return target;
}

void Airplane::setOrigin(const QPointF origin)
{
    this->origin = origin;
}
