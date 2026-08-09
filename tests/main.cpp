#include <QTest>

#include "TestGdbRspCodec.h"
#include "TestNmSymbolParser.h"
#include "TestValueCodec.h"
#include "TestWatchModel.h"
#include "TestElfTargetMatcher.h"
#include "TestWatchPlanBuilder.h"
#include "TestTraceBuffer.h"
#include "TestTraceEventLog.h"

// Runs every suite with QTest::qExec and ORs the exit statuses, so a failure
// in one suite doesn't stop the others from reporting (docs/variable_watcher_plan.md
// Bolum 13). Add a new `{ TestX t; status |= QTest::qExec(&t, argc, argv); }`
// line here whenever a new pure class gets a test suite.
int main(int argc, char **argv)
{
    int status = 0;
    { TestGdbRspCodec t;      status |= QTest::qExec(&t, argc, argv); }
    { TestNmSymbolParser t;   status |= QTest::qExec(&t, argc, argv); }
    { TestValueCodec t;       status |= QTest::qExec(&t, argc, argv); }
    { TestWatchModel t;       status |= QTest::qExec(&t, argc, argv); }
    { TestElfTargetMatcher t; status |= QTest::qExec(&t, argc, argv); }
    { TestWatchPlanBuilder t; status |= QTest::qExec(&t, argc, argv); }
    { TestTraceBuffer t;      status |= QTest::qExec(&t, argc, argv); }
    { TestTraceEventLog t;    status |= QTest::qExec(&t, argc, argv); }
    return status;
}
