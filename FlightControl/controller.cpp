#include "controller.h"

#include <random>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QHBoxLayout>

Controller::Controller(int width, int height)
{
    // Create the scene
    scene = new QGraphicsScene(this);
    setSceneRect(0, 0, width, height);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);


    setFixedSize(width, height);
//    fitInView(sceneRect(), Qt::KeepAspectRatio);
    setScene(scene);
    setMouseTracking(true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    focusedPlane = nullptr;
    selectedAirport1 = nullptr;
    selectedAirport2 = nullptr;

    // Call update() every 50 miliseconds
    static QTimer t;
    connect(&t, SIGNAL(timeout()), this, SLOT(update()));
    t.start(50);

    // Call spawnPlanes() every 15 seconds
    static QTimer t1;
    connect(&t1, SIGNAL(timeout()), this, SLOT(spawnPlanes()));
    t1.start(15000);

    scaleCounter = 0;
    planeCounter = 0;
    buildAirport = false;

    log = new Log(this);
    log->show();

    connect(log, SIGNAL(isClicked()),
            this, SLOT(save()));

    run(width, height);
}

void Controller::run(int width, int height)
{

    // READ AIRPORT DATA FROM JSON
    QFile file(":/data/airports.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString val = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
//    qDebug() << doc;
    QJsonObject jsonObj = doc.object();

    QJsonValue map = jsonObj.value("map");
    QPixmap background(map.toString());
    setSceneRect(0, 0, background.width(), background.height());
//    background = background.scaled(width, height, Qt::IgnoreAspectRatio);
    setCacheMode(QGraphicsView::CacheBackground);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    double scaleX = static_cast<double>(width) / background.width();
    double scaleY = static_cast<double>(height) / background.height();
    scale(scaleX, scaleY);
    setBackgroundBrush(background);


    QJsonValue value = jsonObj.value("airports");
//    qDebug() << value.toArray()[0].toObject()["name"].toString();
    QJsonArray array = value.toArray();


    for(int i = 0; i < array.size(); i++) {
        // Create an airport and add it to scene
        QJsonObject airport = array[i].toObject();
        airports.push_back(new Airport(airport["name"].toString()));
        QJsonArray position = airport["position"].toArray();
        QPoint pos(position[0].toDouble(), position[1].toDouble());
        airports[i]->setPos(pos);

//        QPointF tmp = mapToScene(pos);
//        airports[i]->setFlag(QGraphicsItem::ItemIsSelectable, true);
        scene->addItem(airports[i]);
//        scene->addEllipse(tmp.x()-airports[i]->radarRadius/2, tmp.y()-airports[i]->radarRadius/2, airports[i]->radarRadius,airports[i]->radarRadius);
    }

    // READ AIRPLANE DATA FROM JSON
    QFile file1(":/data/airplanes.json");
    file1.open(QIODevice::ReadOnly | QIODevice::Text);
    val = file1.readAll();
    doc = QJsonDocument::fromJson(val.toUtf8());
//    qDebug() << doc;
    jsonObj = doc.object();
    value = jsonObj.value("airplanes");
    array = value.toArray();

    for(int i = 0; i < array.size(); i++){
        QJsonObject airplane = array[i].toObject();
        AirplaneData data;
        data.image = airplane["image"].toString();
        data.fuelCap = airplane["fuelCap"].toDouble();
        data.fuelUse = airplane["fuelUse"].toDouble();
        data.speed = airplane["speed"].toDouble();
        data.maxAngle = airplane["maxAngle"].toDouble();
        Airplane::data.push_back(data);
    }

    show();
}

void Controller::saveAirportData(QString file)
{
    QFile f(file);
    f.open(QIODevice::ReadOnly | QIODevice::Text);

    QString val = f.readAll();
    f.close();
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
//                qDebug() << doc;

    QJsonObject jsonObj = doc.object();
    QJsonArray arr;

    for(Airport* a : airports){

        QJsonObject newAirport;
        newAirport.insert("name", a->getName());
        QPointF p(a->pos());

        QJsonArray newArr;
        newArr.push_back(p.x());
        newArr.push_back(p.y());
        newAirport.insert("position", newArr);

        arr.append(newAirport);
    }

    jsonObj.insert("airports", arr);

    QJsonDocument d(jsonObj);
//                qDebug() << d.toJson();

    f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    f.write(d.toJson());
}

static double distanceSquared(QPointF p, QPointF q)
{
    return (p.x() - q.x()) * (p.x() - q.x()) + (p.y() - q.y()) * (p.y() - q.y());
}

Airport* Controller::findClosestAirport(const QPointF& airplanePos)
{
    // This airplane's target is the closest airport
    auto closest = std::min_element(airports.cbegin(), airports.cend(),
                                    [airplanePos](Airport* a1, Airport* a2)
                                    {return distanceSquared(a1->pos(), airplanePos) <
                                            distanceSquared(a2->pos(), airplanePos);});

//    qDebug() << (*closest)->name;
    return *closest;
}

void Controller::mousePressEvent(QMouseEvent *event)
{
    // Spawn a new airplane (or an airport) on the clicked location (this is for testing purposes)
    if(event->button() == Qt::RightButton) {

        if(!buildAirport){

            Airport* closest;
            if(selectedAirport2) closest = selectedAirport2;
            else if(selectedAirport1) closest = selectedAirport1;
            else closest = findClosestAirport(mapToScene(event->pos()));

            Airplane* plane = new Airplane(mapToScene(event->pos()), closest->pos());
            connect(plane, SIGNAL(finished(QString,bool)),
                    this, SLOT(planeFinished(QString,bool)));
            closest->planes.push_back(plane);
            scene->addItem(plane);
            QString s = "Flight-" + QString::number(plane->flightNo) + " : nowhere "
                    + " --> " + closest->getName();
            emit flightInfo(s);
    //        qDebug() <<mapToScene(event->pos());

        }else{

            QString name(log->txtEdit->toPlainText());
            Airport* port = new Airport(name);
            port->setPos(mapToScene(event->pos()));
//            port->setFlag(QGraphicsItem::ItemIsSelectable, true);
            airports.push_back(port);
            scene->addItem(port);


        }

    } else if(event->button() == Qt::LeftButton) {

        for(Airport* airport : airports) {
            // Check if this airport is clicked
            if(airport->sceneBoundingRect().contains((mapToScene(event->pos())))){
                if(!selectedAirport1){
//                    qDebug() << "Selected airport: " << airport->getName();
                    selectedAirport1 = airport;
                    selectedAirport1->select();
                }else if(selectedAirport1 != airport && !selectedAirport2){
//                    qDebug() << "Selected airport: " << airport->getName();
                    selectedAirport2 = airport;
                    selectedAirport2->select();
                }
                return;
            }


            // Check if airplane is clicked
            foreach (Airplane* plane, airport->planes) {
    //            qDebug() << plane->boundingRect();
                if(plane->sceneBoundingRect().contains((mapToScene(event->pos())))){

                    if(focusedPlane) focusedPlane->setState(State::FLYING);

                    focusedPlane = plane;

                    //focused_plane->setState(State::MANUAL);
                    //qDebug() << "Manual control of flight " << focused_plane->flightNo;
                    return;
                }
            }
        }

        if(focusedPlane){
            focusedPlane->setState(State::FLYING);
            focusedPlane = nullptr;
        }

        if(selectedAirport1){
            selectedAirport1->deselect();
            selectedAirport1 = nullptr;
        }
        if(selectedAirport2){
            selectedAirport2->deselect();
            selectedAirport2 = nullptr;
        }


    } else if(event->button() == Qt::MiddleButton && !focusedPlane){
        // Store original position.
        originX = event->x();
        originY = event->y();
    }
}

void Controller::mouseMoveEvent(QMouseEvent *event)
{
    if(!focusedPlane){
        emit airplaneInfo(QString());
        for(auto& item: scene->items()) {
            if(item->sceneBoundingRect().contains(mapToScene(event->pos()))) {
                Airplane* plane = dynamic_cast<Airplane*>(item);
                if(plane) {
                    QString s = "Flight-" + QString::number(plane->flightNo) + ", fuel left: " + QString::number(plane->getFuel()) + ", Control: AUTO";
                    emit airplaneInfo(s);
                }
            }
        }
    }

    if((event->buttons() & Qt::MidButton) && !focusedPlane){
        QPointF oldp = mapToScene(originX, originY);
        QPointF newp = mapToScene(event->pos());
        QPointF translation = newp - oldp;

        translate(translation.x(), translation.y());

        originX = event->x();
        originY = event->y();
    }
}

void Controller::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Left){

       if(focusedPlane && focusedPlane->getState() == State::MANUAL){
            focusedPlane->steer(-focusedPlane->maxAngle);
       }

    }else if(event->key() == Qt::Key_Right){

        if(focusedPlane && focusedPlane->getState() == State::MANUAL){
             focusedPlane->steer(focusedPlane->maxAngle);
        }

    }else if(event->key() == Qt::Key_Up){

        if(focusedPlane && focusedPlane->getState() != State::CRASHED)
            focusedPlane->setState(State::FLYING);

        //focused_plane = nullptr;

    }else if(event->key() == Qt::Key_Down){

        if(focusedPlane && focusedPlane->getState() == State::MANUAL){
            // land on selected airport or automaticaly find a nearest airport to land
            auto closest = selectedAirport1 ? selectedAirport1.data() : findClosestAirport(focusedPlane->pos());
            // Move this plane from previous target airport to closest airport
            auto targetPos = focusedPlane->getTarget();
            auto currentTargetIt = std::find_if(airports.begin(), airports.end(),
                                                  [targetPos](Airport* a){return a->pos() == targetPos;});
            Airport* currentTarget = *currentTargetIt;
            currentTarget->planes.erase(std::remove(currentTarget->planes.begin(), currentTarget->planes.end(), focusedPlane));

            closest->planes.push_back(focusedPlane);

            focusedPlane->setTarget(closest->pos());

            closest->currentPlane = focusedPlane;
            focusedPlane->setState(State::FLYING);
            focusedPlane = nullptr;
        }

    }else if(event->key() == Qt::Key_F){

        if(focusedPlane){
            if(focusedPlane->getState() == State::MANUAL) focusedPlane->setState(State::FLYING);
            else focusedPlane->setState(State::MANUAL);
        }

    }else if(event->key() == Qt::Key_1){
        if(selectedAirport1 && selectedAirport2){
            Airplane* plane = new Airplane(selectedAirport1->pos(), selectedAirport2->pos(), planeCounter);
            QString s = "Flight-" + QString::number(plane->flightNo) + " : " +
                    selectedAirport1->getName() + " --> " + selectedAirport2->getName();
            emit flightInfo(s);
            selectedAirport2->planes.push_back(plane);
            scene->addItem(plane);
        }

    }else if(event->key() == Qt::Key_2){
        if(++planeCounter >= Airplane::data.size()) planeCounter = -1;
        qDebug() << "Selected plane type " << planeCounter << "(-1 for auto)";

    }else if(event->key() == Qt::Key_3){
        if(--planeCounter < -1) planeCounter = Airplane::data.size()-1;
        qDebug() << "Selected plane type " << planeCounter << "(-1 for auto)";

    }else if(event->key() == Qt::Key_B){
        buildAirport = !buildAirport;
        if(buildAirport)
            qDebug() << "Create airport ";
        else
            qDebug() << "Create airplane ";

    }else if(event->key() == Qt::Key_S){

        save();

    }else if(event->key() == Qt::Key_D){

        if(focusedPlane){
            qDebug() << "Deleted flight-" + QString::number(focusedPlane->flightNo);
            focusedPlane->deleteLater();
            focusedPlane = nullptr;
        }

        if(selectedAirport1){
            qDebug() << "Deleted airport " + selectedAirport1->getName();
            airports.erase(std::remove(airports.begin(), airports.end(), selectedAirport1));
            for(Airplane* p : selectedAirport1->planes){
                Airport* closest = findClosestAirport(p->pos());
                closest->planes.push_back(p);
                p->setTarget(closest->pos());
            }
        }
        if(selectedAirport2){
            qDebug() << "Deleted airport " + selectedAirport2->getName();
            airports.erase(std::remove(airports.begin(), airports.end(), selectedAirport2));
            for(Airplane* p : selectedAirport2->planes){
                Airport* closest = findClosestAirport(p->pos());
                closest->planes.push_back(p);
                p->setTarget(closest->pos());
            }
        }

        if(selectedAirport1){
            selectedAirport1->deleteLater();
        }
        if(selectedAirport2){
            selectedAirport2->deleteLater();
        }
        selectedAirport1 = nullptr;
        selectedAirport2 = nullptr;
    }
}

void Controller::wheelEvent(QWheelEvent *event)
{

    QPointF oldp = mapToScene(event->pos());

    if(event->delta() > 0){
        scaleCounter++;
        scale(1.25, 1.25);
    }else{
        if(scaleCounter > 0) {
            scaleCounter--;
            scale(0.8, 0.8);
        }
    }

    QPointF newp = mapToScene(event->pos());

    QPointF translation = newp - oldp;

    translate(translation.x(), translation.y());
}

void Controller::update()
{
    scene->update(sceneRect());

    if(focusedPlane){
        originX = viewport()->size().width()/2;
        originY = viewport()->size().height()/2;

        QPointF oldp(mapToScene(originX, originY));
        QPointF newp(focusedPlane->pos().x(), focusedPlane->pos().y());
        QPointF translation = oldp - newp;

        translate(translation.x(), translation.y());

        emit airplaneInfo(QString());
        QString s = "Flight-" + QString::number(focusedPlane->flightNo) + ", fuel left: " + QString::number(focusedPlane->getFuel()) + ", Control: " +
                ((focusedPlane->getState() == State::MANUAL) ? "MANUAL" : "AUTO");
        emit airplaneInfo(s);

    }
}

void Controller::spawnPlanes()
{

    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<int> randomDiscrete(1, airports.size()-1);

    int num = randomDiscrete(gen);

    for(int i = 0; i < num; i++) {

            int randomIdx = randomDiscrete(gen);
//            qDebug() << randomIdx;
            int targetIdx = (i + randomIdx) % airports.size();
//            qDebug() << targetIdx;

            Airplane* plane = new Airplane(airports[i]->pos(), airports[targetIdx]->pos());
            connect(plane, SIGNAL(finished(QString,bool)),
                    this, SLOT(planeFinished(QString,bool)));
            QString s = "Flight-" + QString::number(plane->flightNo) + " : " + airports[i]->getName()
                    + " --> " + airports[targetIdx]->getName();
            emit flightInfo(s);
            airports[targetIdx]->planes.push_back(plane);
            scene->addItem(plane);
    }

    std::random_shuffle(airports.begin(), airports.end());

}

void Controller::planeFinished(const QString &s, bool crashed)
{
    emit landingInfo(s, crashed);
}

void Controller::save()
{
    saveAirportData("../FlightControl/data/airports.json");
    qDebug() << "Saved";
}
