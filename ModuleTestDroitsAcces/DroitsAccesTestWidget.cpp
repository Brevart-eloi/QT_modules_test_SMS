#include "DroitsAccesTestWidget.h"

#include <QLineEdit>
#include <QTimeEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDateTime>

// ============================================================================
//  Constructeur
// ============================================================================

DroitsAccesTestWidget::DroitsAccesTestWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Test des droits d'acces - Badge enregistre x Plage horaire (E2)");
    resize(760, 720);

    buildUi();

    // Plage horaire par defaut : 07:00 - 21:00 (sujet : labos accessibles de 7h a 21h)
    m_editStart->setTime(QTime(7, 0));
    m_editEnd->setTime(QTime(21, 0));
    syncScheduleFromUi();

    // Quelques badges enregistres de demonstration (modifiables)
    m_checker.addBadge("04a1b2c3", "Dupont (enseignant)");
    m_checker.addBadge("1a2b3c4d", "Martin (technicien)");
    refreshTable();

    // Heure simulee initialisee a l'heure courante
    m_editTestTime->setTime(QTime::currentTime());

    log("=== Test des droits d'acces ===");
    log("Regle : acces accorde si le badge est enregistre ET l'heure est dans la plage autorisee.");
}

// ============================================================================
//  Construction de l'IHM
// ============================================================================

void DroitsAccesTestWidget::buildUi()
{
    QVBoxLayout *main = new QVBoxLayout(this);

    // ---- Plage horaire autorisee ----
    {
        QGroupBox   *grp  = new QGroupBox("Plage horaire autorisee");
        QHBoxLayout *row  = new QHBoxLayout(grp);
        m_editStart = new QTimeEdit();  m_editStart->setDisplayFormat("HH:mm");
        m_editEnd   = new QTimeEdit();  m_editEnd->setDisplayFormat("HH:mm");
        row->addWidget(new QLabel("Debut :"));
        row->addWidget(m_editStart);
        row->addSpacing(20);
        row->addWidget(new QLabel("Fin :"));
        row->addWidget(m_editEnd);
        row->addStretch();
        row->addWidget(new QLabel("(hors de cette plage, acces refuse meme si badge valide)"));
        main->addWidget(grp);

        connect(m_editStart, &QTimeEdit::timeChanged, this, &DroitsAccesTestWidget::onScheduleChanged);
        connect(m_editEnd,   &QTimeEdit::timeChanged, this, &DroitsAccesTestWidget::onScheduleChanged);
    }

    // ---- Badges enregistres (liste blanche) ----
    {
        QGroupBox   *grp = new QGroupBox("Badges enregistres (liste blanche)");
        QVBoxLayout *v   = new QVBoxLayout(grp);

        m_table = new QTableWidget(0, 2);
        m_table->setHorizontalHeaderLabels({ "UID badge", "Nom du porteur" });
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setColumnWidth(0, 200);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setMaximumHeight(150);
        v->addWidget(m_table);

        QHBoxLayout *addRow = new QHBoxLayout();
        m_editUid  = new QLineEdit();  m_editUid->setPlaceholderText("UID (hex, ex. 04a1b2c3)");
        m_editName = new QLineEdit();  m_editName->setPlaceholderText("Nom du porteur");
        QPushButton *btnAdd = new QPushButton("Ajouter / Mettre a jour");
        QPushButton *btnDel = new QPushButton("Supprimer la selection");
        addRow->addWidget(m_editUid);
        addRow->addWidget(m_editName);
        addRow->addWidget(btnAdd);
        addRow->addWidget(btnDel);
        v->addLayout(addRow);
        main->addWidget(grp);

        connect(btnAdd, &QPushButton::clicked, this, &DroitsAccesTestWidget::onAddBadge);
        connect(btnDel, &QPushButton::clicked, this, &DroitsAccesTestWidget::onRemoveBadge);
    }

    // ---- Test d'un acces ----
    {
        QGroupBox   *grp = new QGroupBox("Test d'un acces");
        QFormLayout *f   = new QFormLayout(grp);

        m_editTestUid = new QLineEdit();
        m_editTestUid->setPlaceholderText("UID du badge presente");
        f->addRow("UID badge :", m_editTestUid);

        QHBoxLayout *timeRow = new QHBoxLayout();
        m_chkNow = new QCheckBox("Utiliser l'heure actuelle");
        m_chkNow->setChecked(true);
        m_editTestTime = new QTimeEdit();
        m_editTestTime->setDisplayFormat("HH:mm");
        m_editTestTime->setEnabled(false);
        timeRow->addWidget(m_chkNow);
        timeRow->addSpacing(15);
        timeRow->addWidget(new QLabel("Heure simulee :"));
        timeRow->addWidget(m_editTestTime);
        timeRow->addStretch();
        f->addRow("Moment du test :", timeRow);

        QPushButton *btnCheck = new QPushButton("Verifier l'acces");
        btnCheck->setMinimumHeight(38);
        btnCheck->setStyleSheet("font-weight:bold;");
        f->addRow(btnCheck);

        m_lblResult = new QLabel("---");
        m_lblResult->setStyleSheet("font-size:16px; font-weight:bold; color:gray;");
        f->addRow("Resultat :", m_lblResult);

        m_lblDetails = new QLabel("");
        m_lblDetails->setStyleSheet("color:#555;");
        f->addRow("Details :", m_lblDetails);

        main->addWidget(grp);

        connect(btnCheck, &QPushButton::clicked, this, &DroitsAccesTestWidget::onCheckAccess);
        connect(m_chkNow, &QCheckBox::toggled,   this, &DroitsAccesTestWidget::onUseCurrentTimeToggled);
    }

    // ---- Journal ----
    main->addWidget(new QLabel("Journal :"));
    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setStyleSheet("font-family: Consolas, monospace; font-size: 11px;");
    main->addWidget(m_log, 1);
}

// ============================================================================
//  Liste blanche
// ============================================================================

void DroitsAccesTestWidget::refreshTable()
{
    const QList<AuthorizedBadge> list = m_checker.badges();
    m_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(list[i].uid));
        m_table->setItem(i, 1, new QTableWidgetItem(list[i].name));
    }
}

void DroitsAccesTestWidget::onAddBadge()
{
    const QString uid  = m_editUid->text().trimmed();
    const QString name = m_editName->text().trimmed();
    if (uid.isEmpty()) {
        log("Ajout refuse : UID vide.");
        return;
    }
    m_checker.addBadge(uid, name.isEmpty() ? "(sans nom)" : name);
    refreshTable();
    log(QString("Badge enregistre : %1 -> %2")
            .arg(BadgeAccessChecker::normalizeUid(uid), name.isEmpty() ? "(sans nom)" : name));
    m_editUid->clear();
    m_editName->clear();
}

void DroitsAccesTestWidget::onRemoveBadge()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        log("Suppression : aucune ligne selectionnee.");
        return;
    }
    const QString uid = m_table->item(row, 0)->text();
    m_checker.removeBadge(uid);
    refreshTable();
    log(QString("Badge supprime : %1").arg(uid));
}

// ============================================================================
//  Plage horaire
// ============================================================================

void DroitsAccesTestWidget::syncScheduleFromUi()
{
    m_checker.setSchedule(m_editStart->time(), m_editEnd->time());
}

void DroitsAccesTestWidget::onScheduleChanged()
{
    syncScheduleFromUi();
    log(QString("Plage horaire autorisee : %1 - %2")
            .arg(m_editStart->time().toString("HH:mm"),
                 m_editEnd->time().toString("HH:mm")));
}

void DroitsAccesTestWidget::onUseCurrentTimeToggled(bool checked)
{
    // Si on utilise l'heure actuelle, le champ "heure simulee" est desactive
    m_editTestTime->setEnabled(!checked);
    if (checked)
        m_editTestTime->setTime(QTime::currentTime());
}

// ============================================================================
//  Verification de l'acces
// ============================================================================

void DroitsAccesTestWidget::onCheckAccess()
{
    const QString uid = m_editTestUid->text().trimmed();
    if (uid.isEmpty()) {
        log("Test refuse : UID a tester vide.");
        return;
    }

    syncScheduleFromUi();

    const QTime when = m_chkNow->isChecked() ? QTime::currentTime()
                                             : m_editTestTime->time();
    const AccessResult r = m_checker.check(uid, when);

    // Resultat principal
    if (r.granted) {
        m_lblResult->setText(QString("ACCES AUTORISE  —  %1").arg(r.userName));
        m_lblResult->setStyleSheet("font-size:16px; font-weight:bold; color:#2e7d32;");
    } else {
        m_lblResult->setText(QString("ACCES REFUSE  —  %1").arg(r.reason));
        m_lblResult->setStyleSheet("font-size:16px; font-weight:bold; color:#b71c1c;");
    }

    // Detail des deux conditions
    m_lblDetails->setText(QString("Badge enregistre : %1   |   Dans la plage horaire : %2   (heure testee : %3)")
            .arg(r.badgeKnown     ? "OUI" : "NON",
                 r.inScheduleSlot ? "OUI" : "NON",
                 when.toString("HH:mm")));

    log(QString("[%1] UID %2 a %3 -> %4%5")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss"),
                 BadgeAccessChecker::normalizeUid(uid),
                 when.toString("HH:mm"),
                 r.granted ? "AUTORISE" : "REFUSE",
                 r.granted ? QString(" (%1)").arg(r.userName)
                           : QString(" — %1").arg(r.reason)));
}

// ============================================================================
//  Journal
// ============================================================================

void DroitsAccesTestWidget::log(const QString &msg)
{
    m_log->append(QString("[%1] %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), msg));
}
