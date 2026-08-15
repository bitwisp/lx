#pragma once

#include "lx/domain/Result.h"

#include <systemd/sd-journal.h>

namespace lx::linux {

class SdJournal final {
public:
    SdJournal() noexcept = default;
    ~SdJournal();

    SdJournal(const SdJournal&) = delete;
    SdJournal& operator=(const SdJournal&) = delete;
    SdJournal(SdJournal&& other) noexcept;
    SdJournal& operator=(SdJournal&& other) noexcept;

    [[nodiscard]] static Result<SdJournal> openLocal();
    [[nodiscard]] sd_journal* get() const noexcept;

private:
    explicit SdJournal(sd_journal* journal) noexcept;

    sd_journal* journal_ = nullptr;
};

class JournalCursor final {
public:
    JournalCursor() noexcept = default;
    ~JournalCursor();

    JournalCursor(const JournalCursor&) = delete;
    JournalCursor& operator=(const JournalCursor&) = delete;
    JournalCursor(JournalCursor&& other) noexcept;
    JournalCursor& operator=(JournalCursor&& other) noexcept;

    [[nodiscard]] char** put() noexcept;
    [[nodiscard]] const char* get() const noexcept;

private:
    char* cursor_ = nullptr;
};

} // namespace lx::linux
