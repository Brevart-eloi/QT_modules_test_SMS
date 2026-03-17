#pragma once
#include "alarmcore_global.h"
#include <qstring.h>
#include <QObject>

class ALARMCORE_EXPORT AlarmNotifierStrategy : public QObject
{
	Q_OBJECT

public:
	AlarmNotifierStrategy(QObject * parent = nullptr) : QObject(parent) {}
	virtual bool sendAlert(QString description) = 0;
};

