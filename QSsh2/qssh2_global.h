#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(QSSH2_LIB)
#  define QSSH2_EXPORT Q_DECL_EXPORT
# else
#  define QSSH2_EXPORT Q_DECL_IMPORT
# endif
#else
# define QSSH2_EXPORT
#endif
