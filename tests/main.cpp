#include <QTest>

#include "TestGdbRspCodec.h"
#include "TestNmSymbolParser.h"
#include "TestValueCodec.h"
#include "TestWatchModel.h"
#include "TestElfTargetMatcher.h"
#include "TestWatchPlanBuilder.h"
#include "TestWatchSampler.h"
#include "TestTraceBuffer.h"
#include "TestTraceEventLog.h"
#include "TestTraceRecorderPlayer.h"
#include "TestWatchProfile.h"
#include "TestTimeSeriesRuleEngine.h"
#include "TestWatchPresetMatcher.h"
#include "TestRamBudget.h"
#include "TestRateCheck.h"
#include "TestModelFootprint.h"
#include "TestModelFitCheck.h"

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
    { TestWatchSampler t; status |= QTest::qExec(&t, argc, argv); }
    { TestTraceBuffer t;      status |= QTest::qExec(&t, argc, argv); }
    { TestTraceEventLog t;    status |= QTest::qExec(&t, argc, argv); }
    { TestTraceRecorderPlayer t; status |= QTest::qExec(&t, argc, argv); }
    { TestWatchProfile t;     status |= QTest::qExec(&t, argc, argv); }
    { TestTimeSeriesRuleEngine t; status |= QTest::qExec(&t, argc, argv); }
    { TestWatchPresetMatcher t;   status |= QTest::qExec(&t, argc, argv); }
    { TestRamBudget t;            status |= QTest::qExec(&t, argc, argv); }
    { TestRateCheck t;            status |= QTest::qExec(&t, argc, argv); }
    { TestModelFootprint t;       status |= QTest::qExec(&t, argc, argv); }
    { TestModelFitCheck t;        status |= QTest::qExec(&t, argc, argv); }
    return status;
}
