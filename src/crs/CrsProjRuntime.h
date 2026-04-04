#pragma once

#include <QString>

#ifdef LASVIEWERCRS_HAS_PROJ
struct pj_ctx;
typedef struct pj_ctx PJ_CONTEXT;
#endif

namespace lasviewer::crs
{
#ifdef LASVIEWERCRS_HAS_PROJ
void configureProjSearchPaths(PJ_CONTEXT* context);
QString projContextErrorMessage(PJ_CONTEXT* context, const QString& fallbackMessage = QString());
#endif
}
