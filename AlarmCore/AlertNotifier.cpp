#include "AlertNotifier.h"
#include <algorithm>

AlertNotifier::AlertNotifier(QObject* parent)
    : LaboAlertEventListener(parent)
{
}

void AlertNotifier::onAlert(QString description)
{
    for (AlarmNotifierStrategy* strategy : notificationStrategies)
    {
        strategy->sendAlert(description);
    }
}

void AlertNotifier::addNotificationStrategy(AlarmNotifierStrategy* strategy)
{
    notificationStrategies.push_back(strategy);
}

void AlertNotifier::removeNotificationStrategy(AlarmNotifierStrategy* strategy)
{
    notificationStrategies.erase(
        std::remove(notificationStrategies.begin(), notificationStrategies.end(), strategy),
        notificationStrategies.end()
    );
}