#include "lx/linux/systemd/SdJournal.h"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <system_error>
#include <utility>

namespace lx::linux {
namespace {

Error openError(const int status)
{
    const int number = status < 0 ? -status : status;
    ErrorCode code = ErrorCode::Unavailable;
    if (number == EACCES || number == EPERM) {
        code = ErrorCode::PermissionDenied;
    }
    return {code,
            "Unable to open the local journal: " +
                std::system_category().message(number),
            number,
            "sd-journal",
            "open local journal"};
}

} // namespace

SdJournal::SdJournal(sd_journal* journal) noexcept : journal_(journal) {}

SdJournal::~SdJournal()
{
    sd_journal_close(journal_);
}

SdJournal::SdJournal(SdJournal&& other) noexcept
    : journal_(std::exchange(other.journal_, nullptr))
{
}

SdJournal& SdJournal::operator=(SdJournal&& other) noexcept
{
    if (this != &other) {
        sd_journal_close(journal_);
        journal_ = std::exchange(other.journal_, nullptr);
    }
    return *this;
}

Result<SdJournal> SdJournal::openLocal()
{
    sd_journal* journal = nullptr;
    const int status = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (status < 0) {
        sd_journal_close(journal);
        return Result<SdJournal>::failure(openError(status));
    }
    return Result<SdJournal>::success(SdJournal{journal});
}

sd_journal* SdJournal::get() const noexcept
{
    return journal_;
}

JournalCursor::~JournalCursor()
{
    std::free(cursor_);
}

JournalCursor::JournalCursor(JournalCursor&& other) noexcept
    : cursor_(std::exchange(other.cursor_, nullptr))
{
}

JournalCursor& JournalCursor::operator=(JournalCursor&& other) noexcept
{
    if (this != &other) {
        std::free(cursor_);
        cursor_ = std::exchange(other.cursor_, nullptr);
    }
    return *this;
}

char** JournalCursor::put() noexcept
{
    std::free(cursor_);
    cursor_ = nullptr;
    return &cursor_;
}

const char* JournalCursor::get() const noexcept
{
    return cursor_;
}

} // namespace lx::linux
