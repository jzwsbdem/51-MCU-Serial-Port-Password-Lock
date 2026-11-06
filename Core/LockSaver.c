#include "LockSaver.h"
#include "LockConfig.h"
#include "LockSystem.h"
#include "LockInput.h"
#include "Tools/E2PROM.h"
#include "Tools/Timer.h"

#define RECORD_SIZE 11
#define BANK_A 0x20
#define BANK_B 0x40
#define COMMIT 0xA5
#define SAVE_TIMEOUT 90UL
#define RECORD_SEQUENCE 1
#define RECORD_PASSWORD 2
#define RECORD_ERRORS 8
#define RECORD_PHASE 9
#define RECORD_CRC 10

static unsigned char xdata record[RECORD_SIZE];
static unsigned long xdata saveStart;
static unsigned char activeBank, sequence, position, stage;
static bit dirty, changingPassword, writingPassword, failed, readFailed;

static unsigned char SavedPhase()
{
    if (globalLockState == STATE_WAIT || globalLockState == STATE_PROTECT)
        return (unsigned char)globalLockState;
    return STATE_LOCKED;
}

static unsigned char Checksum()
{
    unsigned char i, j, crc = 0;
    for (i = 1; i < RECORD_SIZE - 1; i++)
    {
        crc ^= record[i];
        for (j = 0; j < 8; j++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
    }
    return crc;
}

static bit ReadBank(unsigned char bank)
{
    unsigned char i;
    for (i = 0; i < RECORD_SIZE; i++)
    {
        record[i] = E2PROMByteReader(bank + i);
        if (!E2PROMReadOK())
        {
            readFailed = 1;
            return 0;
        }
    }
    if (record[0] != COMMIT || record[RECORD_CRC] != Checksum() || record[RECORD_ERRORS] > PROTECT_COUNT_MAX)
        return 0;
    if (record[RECORD_PHASE] != STATE_LOCKED && record[RECORD_PHASE] != STATE_WAIT && record[RECORD_PHASE] != STATE_PROTECT)
        return 0;
    for (i = RECORD_PASSWORD; i < RECORD_ERRORS; i++)
        if (record[i] > 9) return 0;
    return 1;
}

void LockSaverInit()
{
    unsigned char i;
    activeBank = BANK_B;
    sequence = position = stage = 0;
    saveStart = 0;
    dirty = changingPassword = writingPassword = failed = readFailed = 0;
    for (i = 0; i < RECORD_SIZE; i++) record[i] = 0;
}

bit LockSaverLoad()
{
    unsigned char i, firstSequence, empty = 0, value;
    bit firstValid, secondValid;
    firstValid = ReadBank(BANK_A);
    firstSequence = record[RECORD_SEQUENCE];
    secondValid = ReadBank(BANK_B);
    if (readFailed) return 0;
    if (firstValid && (!secondValid || (signed char)(firstSequence - record[RECORD_SEQUENCE]) > 0))
    {
        activeBank = BANK_A;
        ReadBank(activeBank);
    }
    else if (secondValid)
        activeBank = BANK_B;
    else
    {
        for (i = 0; i < PASSWORD_LENGTH; i++)
        {
            value = E2PROMByteReader(E2PA_PASSWORD + i);
            if (!E2PROMReadOK()) goto invalid;
            if (value == 0xFF) empty++;
            else if (value <= 9) globalPassword[i] = value;
            else goto invalid;
        }
        if (empty != 0 && empty != PASSWORD_LENGTH) goto invalid;
        value = E2PROMByteReader(E2PA_WRONG_COUNT);
        if (!E2PROMReadOK() || (value != 0xFF && value > PROTECT_COUNT_MAX)) goto invalid;
        globalWrongCount = value == 0xFF ? 0 : value;
        value = E2PROMByteReader(E2PA_STATE);
        if (!E2PROMReadOK() || (value != 0xFF && value > STATE_UPGRADE)) goto invalid;
        globalLockState = (value == STATE_WAIT || value == STATE_PROTECT) ? (LockState)value : STATE_LOCKED;
        dirty = 1;
        return 1;
    }
    if (readFailed) return 0;
    sequence = record[RECORD_SEQUENCE];
    for (i = 0; i < PASSWORD_LENGTH; i++) globalPassword[i] = record[i + RECORD_PASSWORD];
    globalWrongCount = record[RECORD_ERRORS];
    globalLockState = (LockState)record[RECORD_PHASE];
    return 1;
invalid:
    readFailed = failed = 1;
    return 0;
}

bit LockSaverBusy()
{
    return dirty || stage != 0;
}

bit LockSaverFailed()
{
    return failed || readFailed;
}

bit LockSaverReadFailed()
{
    return readFailed;
}

void TriggerSavePassword()
{
    changingPassword = 1;
    dirty = 1;
}

void TriggerSaveWrongCount()
{
    if (failed || globalWrongCount != record[RECORD_ERRORS]) dirty = 1;
}

void TriggerSaveState()
{
    if (failed || SavedPhase() != record[RECORD_PHASE]) dirty = 1;
}

void TaskSavePending()
{
    unsigned char i, bank, value;
    E2PROMWriteTask();
    if (failed && !readFailed && TaskGetTick() - saveStart >= 1000UL)
        dirty = 1;
    if (readFailed)
    {
        if (TaskGetTick() - saveStart < 1000UL) return;
        saveStart = TaskGetTick();
        if (!E2PROMIsReady()) return;
        readFailed = failed = 0;
        if (!LockSaverLoad()) return;
        if (globalLockState == STATE_WAIT || globalLockState == STATE_PROTECT)
        {
            ClearInput();
            globalWrongTimeRemain = globalLockState == STATE_WAIT ? WAIT_TIME : PROTECT_TIME;
            if (!TaskExist(TaskLockTimeRemain)) TaskAdd(1000, TaskLockTimeRemain);
        }
    }
    if (!LockSaverBusy()) return;
    if (stage == 0)
    {
        failed = 0;
        saveStart = TaskGetTick();
        record[0] = COMMIT;
        record[RECORD_SEQUENCE] = sequence + 1;
        writingPassword = changingPassword;
        for (i = 0; i < PASSWORD_LENGTH; i++)
            record[i + RECORD_PASSWORD] = changingPassword ? tempPassword[i] : globalPassword[i];
        record[RECORD_ERRORS] = globalWrongCount;
        record[RECORD_PHASE] = SavedPhase();
        record[RECORD_CRC] = Checksum();
        dirty = 0;
        position = 0;
        stage = 1;
    }
    if (TaskGetTick() - saveStart >= SAVE_TIMEOUT) goto error;
    if (E2PROMIsBusy()) return;
    bank = activeBank == BANK_A ? BANK_B : BANK_A;
    if (stage == 1)
    {
        value = position == 0 ? 0 : record[position];
        if (!E2PROMAsyncByteWriter(bank + position, value)) return;
        saveStart = TaskGetTick();
        if (++position == RECORD_SIZE)
        {
            position = 1;
            stage = 2;
        }
        return;
    }
    if (stage == 2)
    {
        value = E2PROMByteReader(bank + position);
        if (!E2PROMReadOK() || value != record[position]) goto error;
        saveStart = TaskGetTick();
        if (++position == RECORD_SIZE) stage = 3;
        return;
    }
    if (stage == 3)
    {
        if (E2PROMAsyncByteWriter(bank, COMMIT))
        {
            saveStart = TaskGetTick();
            stage = 4;
        }
        return;
    }
    value = E2PROMByteReader(bank);
    if (!E2PROMReadOK() || value != COMMIT) goto error;
    activeBank = bank;
    sequence = record[RECORD_SEQUENCE];
    if (writingPassword)
    {
        for (i = 0; i < PASSWORD_LENGTH; i++) globalPassword[i] = record[i + RECORD_PASSWORD];
        changingPassword = 0;
    }
    stage = 0;
    return;
error:
    stage = 0;
    dirty = changingPassword = writingPassword = 0;
    failed = 1;
}
