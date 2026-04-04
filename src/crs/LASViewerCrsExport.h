#pragma once

#include <QtGlobal>

#if defined(LASVIEWERCRS_SHARED)
#if defined(LASVIEWERCRS_LIBRARY)
#define LASVIEWERCRS_EXPORT Q_DECL_EXPORT
#else
#define LASVIEWERCRS_EXPORT Q_DECL_IMPORT
#endif
#else
#define LASVIEWERCRS_EXPORT
#endif
