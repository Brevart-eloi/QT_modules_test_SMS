#pragma once

#include "alarmcore_global.h"
#include "LaboAlertEventListener.h"      // interface de l'autre étudiant
#include "AlarmNotifierStrategy.h"
#include <deque>
#include <QString>

class ALARMCORE_EXPORT AlertNotifier : public LaboAlertEventListener
{
    Q_OBJECT

private:
    std::deque<AlarmNotifierStrategy*> notificationStrategies;

public:
    explicit AlertNotifier(QObject* parent = nullptr);

    // Implémentation de l'interface LaboAlertEventListener
    void onAlert(QString description) override;

    // Gestion des stratégies
    void addNotificationStrategy(AlarmNotifierStrategy* strategy);
    void removeNotificationStrategy(AlarmNotifierStrategy* strategy);
};