#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(ALARMCORE_LIB)
#  define ALARMCORE_EXPORT Q_DECL_EXPORT
# else
#  define ALARMCORE_EXPORT Q_DECL_IMPORT
# endif
#else
# define ALARMCORE_EXPORT
#endif
