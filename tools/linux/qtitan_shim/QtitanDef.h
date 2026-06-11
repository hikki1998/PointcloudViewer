#pragma once

#include <QtGlobal>

#define QTITAN_NAMESPACE Qtitan
#define QTITAN_BEGIN_NAMESPACE namespace QTITAN_NAMESPACE {
#define QTITAN_END_NAMESPACE }
#define QTITAN_USE_NAMESPACE using namespace QTITAN_NAMESPACE;

#if defined(QTITAN_SHIM_LIBRARY)
#define QTITAN_EXPORT Q_DECL_EXPORT
#else
#define QTITAN_EXPORT Q_DECL_IMPORT
#endif
