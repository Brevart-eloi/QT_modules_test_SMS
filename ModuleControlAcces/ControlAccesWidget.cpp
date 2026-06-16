#include "ControlAccesWidget.h"

#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

// ============================================================================
//  Constructeur / Destructeur
// ============================================================================

ControlAccesWidget::ControlAccesWidget(QWidget *parent)
    : QWidget(parent),
      m_nam       (new QNetworkAccessManager(this)),
      m_gacheTimer(new QTimer(this))
{
    setWindowTitle("Controle d'acces — Badge + Gache (E2)");
    resize(720, 600);

    m_gacheTimer->setSingleShot(true);
    connect(m_gacheTimer, &QTimer::timeout,
            this, &ControlAccesWidget::onGacheTimer);
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &ControlAccesWidget::onAdjudicatorReply);

    QVBoxLayout *main = new QVBoxLayout(this);

    // ── Configuration ──────────────────────────────────────────────────────
    QGroupBox   *grpConf  = new QGroupBox("Configuration");
    QFormLayout *formConf = new QFormLayout(grpConf);

    editModbusIp = new QLineEdit("172.29.240.1");

    spinHttpPort = new QSpinBox();
    spinHttpPort->setRange(1, 65535);
    spinHttpPort->setValue(80);
    spinHttpPort->setToolTip(
        "Port d'ecoute (doit correspondre a la cible configuree dans l'Arduino).\n"
        "Si adjudicator.js tourne deja sur :80 sur la MEME machine, utiliser :8080\n"
        "et pointer l'Arduino vers l'IP de ce PC."
    );

    editAdjudicatorUrl = new QLineEdit("http://172.29.19.193");
    editAdjudicatorUrl->setToolTip("IP + port du serveur adjudicator.js (E1).");

    spinGacheCoil = new QSpinBox();
    spinGacheCoil->setRange(0, 15);
    spinGacheCoil->setValue(0);
    spinGacheCoil->setToolTip("DO0 = Relais 0 = Gache (tableau de cablage)");

    spinGacheDuration = new QSpinBox();
    spinGacheDuration->setRange(1, 30);
    spinGacheDuration->setValue(3);   // 3 s comme adjudicator.js (petPulseCoil 3000 ms)
    spinGacheDuration->setSuffix(" s");

    formConf->addRow("IP PET-7067 (Modbus — gache) :",             editModbusIp);
    formConf->addRow("Port ecoute HTTP (Arduino -> ce serveur) :",  spinHttpPort);
    formConf->addRow("URL adjudicator (base, ex: http://... ) :",   editAdjudicatorUrl);
    formConf->addRow("Adresse DO gache (DO0 = Relais 0) :",         spinGacheCoil);
    formConf->addRow("Duree ouverture gache :",                     spinGacheDuration);

    main->addWidget(grpConf);

    // ── Boutons start / stop ───────────────────────────────────────────────
    QHBoxLayout *srvLayout = new QHBoxLayout();
    btnStart = new QPushButton("Demarrer le service");
    btnStop  = new QPushButton("Arreter");
    btnStop->setEnabled(false);
    btnStart->setMinimumHeight(36);
    btnStop->setMinimumHeight(36);
    srvLayout->addWidget(btnStart);
    srvLayout->addWidget(btnStop);
    srvLayout->addStretch();
    main->addLayout(srvLayout);

    // ── Etat ──────────────────────────────────────────────────────────────
    QGroupBox   *grpState = new QGroupBox("Etat");
    QFormLayout *formSt   = new QFormLayout(grpState);

    labelServerStatus = new QLabel("arrete");
    labelModbusStatus = new QLabel("non connecte");
    labelLastBadge    = new QLabel("---");
    labelGacheStatus  = new QLabel("verrouille");
    labelGacheStatus->setStyleSheet("font-weight: bold; color: #2e7d32;");

    QHBoxLayout *gacheRow = new QHBoxLayout();
    gacheRow->addWidget(labelGacheStatus);
    gacheRow->addSpacing(16);
    btnCloseGache = new QPushButton("Forcer fermeture");
    btnCloseGache->setEnabled(false);
    gacheRow->addWidget(btnCloseGache);
    gacheRow->addStretch();

    formSt->addRow("Serveur HTTP :",  labelServerStatus);
    formSt->addRow("Modbus PET :",    labelModbusStatus);
    formSt->addRow("Dernier acces :", labelLastBadge);
    formSt->addRow("Gache :",         gacheRow);

    main->addWidget(grpState);

    // ── Journal ───────────────────────────────────────────────────────────
    main->addWidget(new QLabel("Journal :"));
    textLog = new QTextEdit();
    textLog->setReadOnly(true);
    textLog->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(textLog);

    // ── Connexions ────────────────────────────────────────────────────────
    connect(btnStart,      &QPushButton::clicked, this, &ControlAccesWidget::onBtnStartClicked);
    connect(btnStop,       &QPushButton::clicked, this, &ControlAccesWidget::onBtnStopClicked);
    connect(btnCloseGache, &QPushButton::clicked, this, &ControlAccesWidget::onBtnCloseGacheClicked);

    log("=== Controle d'acces par badge — E2 ===");
    log("Flux : Arduino → POST /rfid/scan → verif adjudicator → gache Modbus");
    log("Demarrez le service pour commencer.");
}

ControlAccesWidget::~ControlAccesWidget()
{
    // Fermeture de secours avant destruction
    if (m_gacheOpen && m_modbus && m_modbus->state() == QAbstractSocket::ConnectedState) {
        m_modbus->forceSingleCoilFC5(static_cast<quint16>(spinGacheCoil->value()), false);
    }
}

// ============================================================================
//  Start / Stop
// ============================================================================

void ControlAccesWidget::onBtnStartClicked()
{
    // ── HTTP server ──────────────────────────────────────────────────────
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &ControlAccesWidget::onNewConnection);

    const quint16 port = static_cast<quint16>(spinHttpPort->value());
    if (!m_server->listen(QHostAddress::Any, port)) {
        log(QString("ERREUR : impossible d'ecouter sur le port %1 : %2")
                .arg(port).arg(m_server->errorString()));
        log("Verifiez qu'aucun autre programme n'utilise ce port (ex: adjudicator.js sur :80).");
        delete m_server;
        m_server = nullptr;
        return;
    }
    log(QString("Serveur HTTP actif — port %1 — en attente des scans Arduino.").arg(port));
    labelServerStatus->setText(QString("actif (port %1)").arg(port));
    labelServerStatus->setStyleSheet("color: #2e7d32; font-weight: bold;");

    // ── Modbus PET-7067 ───────────────────────────────────────────────────
    m_modbus = new QModbusTcpClient(editModbusIp->text().trimmed(), 502, 1, this);
    connect(m_modbus, &QTcpSocket::connected,
            this, &ControlAccesWidget::onModbusConnected);
    connect(m_modbus, &QTcpSocket::disconnected,
            this, &ControlAccesWidget::onModbusDisconnected);
    m_modbus->connectToHost();

    updateButtons(true);
}

void ControlAccesWidget::onBtnStopClicked()
{
    closeGache();   // force fermeture avant de couper Modbus

    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    if (m_modbus) {
        m_modbus->close();
        m_modbus->deleteLater();
        m_modbus = nullptr;
    }
    m_buffers.clear();

    labelServerStatus->setText("arrete");
    labelServerStatus->setStyleSheet("color: gray;");
    labelModbusStatus->setText("non connecte");
    labelModbusStatus->setStyleSheet("color: gray;");

    log("Service arrete.");
    updateButtons(false);
}

void ControlAccesWidget::onBtnCloseGacheClicked()
{
    closeGache();
}

// ============================================================================
//  Serveur HTTP — reception des POST Arduino (/rfid/scan)
//
//  Le lecteur RFID Arduino envoie :
//    POST /rfid/scan HTTP/1.1
//    Content-Type: application/json
//    Content-Length: N
//    {"card_id":"AABB1234","wiegand_type":26}
// ============================================================================

void ControlAccesWidget::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead,
                this, &ControlAccesWidget::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &ControlAccesWidget::onClientDisconnected);
    }
}

void ControlAccesWidget::onClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    m_buffers[socket] += socket->readAll();
    QByteArray &buf = m_buffers[socket];

    // ── Cherche la fin des headers (CRLF ou LF seul, les deux sont acceptés) ──
    int sep = buf.indexOf("\r\n\r\n");
    if (sep < 0) sep = buf.indexOf("\n\n");
    if (sep < 0) return;   // headers pas encore complets, on attend

    const int bodyOffset = sep + (buf.mid(sep, 4) == "\r\n\r\n" ? 4 : 2);
    const QByteArray headers = buf.left(sep);

    // ── Content-Length ────────────────────────────────────────────────────
    int contentLength = 0;
    for (const QByteArray &line : headers.split('\n')) {
        const QByteArray lower = line.trimmed().toLower();
        if (lower.startsWith("content-length:")) {
            contentLength = lower.mid(15).trimmed().toInt();
            break;
        }
    }

    // Attendre le body complet si Content-Length précisé
    if (contentLength > 0 && buf.size() < bodyOffset + contentLength) return;

    // ── Extraction du body ────────────────────────────────────────────────
    const QByteArray body = (contentLength > 0)
        ? buf.mid(bodyOffset, contentLength)
        : buf.mid(bodyOffset);   // fallback : tout ce qui vient après les headers

    m_buffers.remove(socket);   // on a le body, nettoyage immédiat

    // ── Réponse à l'Arduino (avant tout traitement async) ─────────────────
    sendHttpResponse(socket, 200, R"({"ok":true})");
    socket->disconnectFromHost();

    // ── Extraction du card_id ─────────────────────────────────────────────
    // Format adjudicator : {"card_id":"AABB1234","wiegand_type":26}
    if (body.isEmpty()) {
        log("POST recu mais body vide — ignoré.");
        return;
    }

    const QJsonObject json = QJsonDocument::fromJson(body).object();
    if (json.isEmpty()) {
        log(QString("POST recu — JSON invalide : %1").arg(QString::fromUtf8(body.left(80))));
        return;
    }

    // card_id peut aussi s'appeler uid selon les implémentations
    QString uid = json["card_id"].toString();
    if (uid.isEmpty()) uid = json["uid"].toString();
    uid = uid.trimmed().toUpper();   // normalisation identique a adjudicator.js

    if (uid.isEmpty()) {
        log("POST recu — card_id absent dans le JSON.");
        return;
    }

    handleRfidScan(uid);
}

void ControlAccesWidget::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        m_buffers.remove(socket);
        socket->deleteLater();
    }
}

void ControlAccesWidget::sendHttpResponse(QTcpSocket *socket, int code, const QByteArray &body)
{
    const QString status = (code == 200) ? "200 OK" : "400 Bad Request";
    const QByteArray resp =
        QString("HTTP/1.1 %1\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %2\r\n"
                "Connection: close\r\n"
                "\r\n").arg(status).arg(body.size()).toUtf8()
        + body;
    socket->write(resp);
    socket->flush();
}

// ============================================================================
//  Verification du badge via adjudicator (E1)
//
//  Appel : GET /api/rfid/check/{uid}
//  Reponse adjudicator :
//    { "ok": true, "authorized": true/false, "uid": "...",
//      "owner": "Nom", "message": "Badge autorise / inconnu / desactive" }
// ============================================================================

void ControlAccesWidget::handleRfidScan(const QString &uid)
{
    log(QString("Scan recu : %1 — verification aupres de adjudicator...").arg(uid));
    labelLastBadge->setText(uid + "  (verification...)");
    labelLastBadge->setStyleSheet("color: #555;");

    const QUrl url(editAdjudicatorUrl->text().trimmed()
                   + "/api/rfid/check/" + uid);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->get(req);
    reply->setProperty("uid", uid);
}

void ControlAccesWidget::onAdjudicatorReply(QNetworkReply *reply)
{
    reply->deleteLater();
    const QString uid = reply->property("uid").toString();

    // ── Erreur réseau → refus par sécurité (fail-closed) ──────────────────
    if (reply->error() != QNetworkReply::NoError) {
        log(QString("[%1] ERREUR adjudicator : %2 → acces refuse (fail-safe).")
                .arg(uid, reply->errorString()));
        labelLastBadge->setText(uid + "  — ERREUR RESEAU");
        labelLastBadge->setStyleSheet("color: #b71c1c; font-weight: bold;");
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonObject obj = QJsonDocument::fromJson(data).object();

    if (!obj.value("ok").toBool(true)) {
        log(QString("[%1] Reponse adjudicator invalide.").arg(uid));
        return;
    }

    const bool    authorized = obj["authorized"].toBool(false);
    const QString owner      = obj["owner"].toString("inconnu");
    const QString message    = obj.value("message").toString();

    if (authorized) {
        // ── ACCES AUTORISE — ouvrir la gache ──────────────────────────────
        log(QString("[%1] ACCES AUTORISE — %2").arg(uid, owner));
        labelLastBadge->setText(QString("%1  —  %2  —  AUTORISE").arg(uid, owner));
        labelLastBadge->setStyleSheet("color: #2e7d32; font-weight: bold;");
        openGache();
    } else {
        // ── ACCES REFUSE — ne rien faire ──────────────────────────────────
        const QString raison = message.isEmpty() ? "non autorise" : message;
        log(QString("[%1] ACCES REFUSE — %2 — %3").arg(uid, owner, raison));
        labelLastBadge->setText(QString("%1  —  %2  —  REFUSE").arg(uid, owner));
        labelLastBadge->setStyleSheet("color: #b71c1c; font-weight: bold;");
    }
}

// ============================================================================
//  Gache (relais DO0 sur PET-7067, pulse = ON puis timer → OFF)
//  Equivalent de petPulseCoil(0, 3000) dans adjudicator.js
// ============================================================================

void ControlAccesWidget::openGache()
{
    if (!m_modbus || m_modbus->state() != QAbstractSocket::ConnectedState) {
        log("ATTENTION : PET-7067 non connecte — gache non activee !");
        return;
    }

    const quint16 coil = static_cast<quint16>(spinGacheCoil->value());
    const int     dur  = spinGacheDuration->value();

    m_modbus->forceSingleCoilFC5(coil, true);   // DO0 = ON
    m_gacheOpen = true;
    m_gacheTimer->start(dur * 1000);            // timer → closeGache()

    labelGacheStatus->setText(QString("DEVERROUILLE  (%1 s)").arg(dur));
    labelGacheStatus->setStyleSheet("font-weight: bold; color: #b71c1c;");
    btnCloseGache->setEnabled(true);

    log(QString("Gache ouverte — DO%1 ON — fermeture dans %2 s.")
            .arg(coil).arg(dur));
}

void ControlAccesWidget::closeGache()
{
    m_gacheTimer->stop();
    if (!m_gacheOpen) return;

    if (m_modbus && m_modbus->state() == QAbstractSocket::ConnectedState) {
        m_modbus->forceSingleCoilFC5(static_cast<quint16>(spinGacheCoil->value()), false);
    }
    m_gacheOpen = false;

    labelGacheStatus->setText("verrouille");
    labelGacheStatus->setStyleSheet("font-weight: bold; color: #2e7d32;");
    btnCloseGache->setEnabled(false);

    log("Gache fermee — DO verrouille.");
}

void ControlAccesWidget::onGacheTimer()
{
    closeGache();
}

// ============================================================================
//  Modbus events
// ============================================================================

void ControlAccesWidget::onModbusConnected()
{
    log(QString("PET-7067 connecte (%1) — s'assure que DO%2 est OFF au demarrage.")
            .arg(editModbusIp->text())
            .arg(spinGacheCoil->value()));
    labelModbusStatus->setText("connecte");
    labelModbusStatus->setStyleSheet("color: #2e7d32; font-weight: bold;");

    // Sécurité au démarrage : gâche fermée
    m_modbus->forceSingleCoilFC5(static_cast<quint16>(spinGacheCoil->value()), false);
}

void ControlAccesWidget::onModbusDisconnected()
{
    log("PET-7067 deconnecte — gache physiquement verrouilee par defaut (relais repos).");
    labelModbusStatus->setText("deconnecte");
    labelModbusStatus->setStyleSheet("color: #b71c1c; font-weight: bold;");
    m_gacheOpen = false;
}

// ============================================================================
//  Utilitaires
// ============================================================================

void ControlAccesWidget::log(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append(QString("[%1] %2").arg(ts, msg));
}

void ControlAccesWidget::updateButtons(bool running)
{
    btnStart->setEnabled(!running);
    btnStop->setEnabled(running);
    editModbusIp->setEnabled(!running);
    spinHttpPort->setEnabled(!running);
    editAdjudicatorUrl->setEnabled(!running);
    spinGacheCoil->setEnabled(!running);
    spinGacheDuration->setEnabled(!running);
}
