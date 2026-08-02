#include <QTest>

#include "TestGdbRspCodec.h"

// Runs every suite with QTest::qExec and ORs the exit statuses, so a failure
// in one suite doesn't stop the others from reporting (docs/variable_watcher_plan.md
// Bolum 13). Add a new `{ TestX t; status |= QTest::qExec(&t, argc, argv); }`
// line here whenever a new pure class gets a test suite.
int main(int argc, char **argv)
{
    int status = 0;
    { TestGdbRspCodec t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}
