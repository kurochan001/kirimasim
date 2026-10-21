#include "kirimasim/kernel/files.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kirimasim/kernel/scan.h"

namespace kirimasim::kernel {

namespace {

/// The types of Table 21-6, which are C's modes as they are (21.3.1).
constexpr std::array<std::string_view, 15> kTypes{
    "r", "rb", "w", "wb", "a", "ab", "r+", "r+b", "rb+", "w+", "w+b", "wb+", "a+", "a+b", "ab+"};
constexpr std::uint32_t kFdBit = 0x8000'0000U;
constexpr std::uint32_t kLastMcdBit = 30;  // bit 31 marks an fd (21.3.1)

}  // namespace

Files::Files(Output& output, std::FILE* in) : output_(output), rows_(kFdRow + 3) {
    // STDIN reads the Context's stream; STDOUT and STDERR are the Output's,
    // and a read of them fails as of any file that only writes.
    rows_[kFdRow].f = in;
    rows_[kFdRow].owned = false;
    rows_[kFdRow].reads = true;
    rows_[kFdRow].writes = false;
}

Files::~Files() {
    for (const File& r : rows_)
        if (r.f != nullptr && r.owned) std::fclose(r.f);
}

std::uint32_t Files::place(std::uint32_t row, std::FILE* f) {
    if (rows_.size() <= row) rows_.resize(row + 1);
    rows_[row] = File{};
    rows_[row].f = f;
    rows_[row].serial = ++serial_;
    return row;
}

std::uint32_t Files::openMcd(const std::string& name) {
    std::uint32_t row = 1;
    while (row <= kLastMcdBit && open(row))
        ++row;
    if (row > kLastMcdBit) return 0;
    std::FILE* f = std::fopen(name.c_str(), "w");
    if (f == nullptr) return 0;
    return 1U << place(row, f);
}

std::uint32_t Files::openFd(const std::string& name, std::string_view type, bool& badType) {
    badType = std::find(kTypes.begin(), kTypes.end(), type) == kTypes.end();
    if (badType) return 0;
    std::uint32_t row = kFdRow + 3;  // after STDIN, STDOUT and STDERR
    while (open(row))
        ++row;
    std::FILE* f = std::fopen(name.c_str(), std::string(type).c_str());
    if (f == nullptr) return 0;
    place(row, f);
    // r and the types for update read (SYSTEM_TASKS.md 6.3).
    rows_[row].reads = type.front() == 'r' || type.find('+') != std::string_view::npos;
    rows_[row].writes = type.front() != 'r' || type.find('+') != std::string_view::npos;
    return kFdBit | (row - kFdRow);
}

FileTargets Files::targets(W4 desc) const {
    FileTargets t;
    const auto a = static_cast<std::uint32_t>(desc.a);
    if (static_cast<std::uint32_t>(desc.b) != 0) {
        t.notOpen = true;  // x or z bits
        return t;
    }
    if ((a & kFdBit) != 0) {
        // STDIN takes no output, as C's stream would not (SYSTEM_TASKS.md 6.1).
        const std::uint32_t n = a & ~kFdBit;
        if (n == 1) {
            t.out = true;
        } else if (n == 2) {
            t.err = true;
        } else if (n >= 3 && open(kFdRow + n)) {
            t.files.emplace_back(kFdRow + n, rows_[kFdRow + n].serial);
        } else if (n != 0) {
            t.notOpen = true;
        }
        return t;
    }
    if (a == 0) t.notOpen = true;
    if ((a & 1U) != 0) t.out = true;
    for (std::uint32_t row = 1; row <= kLastMcdBit; ++row) {
        if (((a >> row) & 1U) == 0) continue;
        if (open(row))
            t.files.emplace_back(row, rows_[row].serial);
        else
            t.notOpen = true;
    }
    return t;
}

void Files::write(const FileTargets& t, std::string_view text) {
    if (text.empty()) return;
    if (t.out) output_.write(text);
    if (t.err) output_.writeError(text);
    for (const auto& [row, serial] : t.files) {
        if (!open(row) || rows_[row].serial != serial) continue;
        File& r = rows_[row];
        startWrite(r);
        r.err = std::fwrite(text.data(), 1, text.size(), r.f) == text.size() ? 0 : errno;
    }
}

bool Files::live(const FileTargets& t) const {
    if (t.out || t.err) return true;
    for (const auto& [row, serial] : t.files)
        if (open(row) && rows_[row].serial == serial) return true;
    return false;
}

bool Files::close(W4 desc) {
    const FileTargets t = targets(desc);
    for (const auto& [row, serial] : t.files) {
        std::fclose(rows_[row].f);
        rows_[row] = File{};
    }
    return !t.notOpen;
}

bool Files::flush(W4 desc) {
    const FileTargets t = targets(desc);
    if (t.out) output_.flush();
    for (const auto& [row, serial] : t.files)
        std::fflush(rows_[row].f);
    return !t.notOpen;
}

void Files::flushAll() {
    output_.flush();
    for (const File& r : rows_)
        if (r.f != nullptr && r.writes) std::fflush(r.f);
}

Files::File* Files::reader(W4 desc, bool& notOpen) {
    notOpen = false;
    const auto a = static_cast<std::uint32_t>(desc.a);
    const std::uint32_t n = a & ~kFdBit;
    // STDIN, STDOUT and STDERR are always there (21.3.1).
    if (static_cast<std::uint32_t>(desc.b) == 0 && (a & kFdBit) != 0 &&
        (n <= 2 || open(kFdRow + n)))
        return &rows_[kFdRow + n];
    notOpen = true;
    return nullptr;
}

bool Files::startRead(File& r) {
    r.err = 0;
    if (!r.reads || r.f == nullptr) {
        // What $ungetc pushed back is read all the same; take fails after
        // it (SYSTEM_TASKS.md 6.3).
        if (!r.back.empty()) return true;
        r.err = EBADF;
        return false;
    }
    // C asks for a seek between writing a stream and reading it.
    if (!r.reading && r.writes) std::fseek(r.f, 0, SEEK_CUR);
    r.reading = true;
    return true;
}

void Files::startWrite(File& r) {
    if (!r.reading) return;
    // Write where the next read would have read; what was pushed back goes.
    if (!r.back.empty()) {
        std::fseek(r.f, -static_cast<long>(r.back.size()), SEEK_CUR);
        r.back.clear();
    } else {
        std::fseek(r.f, 0, SEEK_CUR);
    }
    r.reading = false;
}

int Files::take(File& r) {
    if (!r.back.empty()) {
        const auto c = static_cast<unsigned char>(r.back.back());
        r.back.pop_back();
        return c;
    }
    if (!r.reads || r.f == nullptr) {  // a file that only writes
        r.err = EBADF;
        return -1;
    }
    const int c = std::fgetc(r.f);
    if (c != EOF) return c;
    if (std::ferror(r.f) != 0) {
        r.err = errno != 0 ? errno : EIO;
        std::clearerr(r.f);
    } else {
        r.end = true;
    }
    return -1;
}

int Files::getChar(W4 desc, bool& notOpen) {
    File* r = reader(desc, notOpen);
    if (r == nullptr || !startRead(*r)) return -1;
    return take(*r);
}

int Files::ungetChar(int c, W4 desc, bool& notOpen) {
    File* r = reader(desc, notOpen);
    if (r == nullptr) return -1;
    // Any open file takes it, one that only writes too (SYSTEM_TASKS.md
    // 6.3): the next reads return it.
    r->err = 0;
    r->back.push_back(static_cast<char>(c));
    return 0;
}

void Files::readLine(W4 desc, std::size_t max, std::string& line, bool& notOpen) {
    line.clear();
    File* r = reader(desc, notOpen);
    if (r == nullptr || !startRead(*r)) return;
    while (max == 0 || line.size() < max) {
        const int c = take(*r);
        if (c < 0) break;
        line.push_back(static_cast<char>(c));
        if (c == '\n') break;
    }
}

int Files::scan(W4 desc, std::string_view format, std::vector<ScanValue>& out, bool& notOpen) {
    File* r = reader(desc, notOpen);
    if (r == nullptr || !startRead(*r)) return -1;
    class FromFile final : public ScanInput {
    public:
        FromFile(Files& files, File& file) : files_(files), file_(file) {}
        int get() override { return files_.take(file_); }

    private:
        Files& files_;
        File& file_;
    } in(*this, *r);
    std::string unread;
    const int n = scanInput(in, format, out, unread);
    // What the scan looked at and did not use is read next.
    r->back.append(unread.rbegin(), unread.rend());
    return n;
}

void Files::readBytes(W4 desc, std::size_t n, std::string& bytes, bool& notOpen) {
    bytes.clear();
    File* r = reader(desc, notOpen);
    if (r == nullptr || !startRead(*r)) return;
    while (bytes.size() < n) {
        const int c = take(*r);
        if (c < 0) break;
        bytes.push_back(static_cast<char>(c));
    }
}

std::int64_t Files::tell(W4 desc, bool& notOpen) {
    File* r = reader(desc, notOpen);
    if (r == nullptr) return -1;
    r->err = 0;
    if (r->f == nullptr) {
        r->err = ESPIPE;  // STDOUT and STDERR are the Output's, with no position here
        return -1;
    }
    const long p = std::ftell(r->f);
    if (p < 0) {
        r->err = errno;
        return -1;
    }
    // Pushed back before the start, a character has no position of its own.
    return std::max<std::int64_t>(0, p - static_cast<std::int64_t>(r->back.size()));
}

int Files::seek(W4 desc, std::int64_t offset, std::int64_t from, bool& notOpen) {
    File* r = reader(desc, notOpen);
    if (r == nullptr) return -1;
    r->err = 0;
    if (r->f == nullptr) {
        r->err = ESPIPE;
        return -1;
    }
    if (from < 0 || from > 2) {
        r->err = EINVAL;
        return -1;
    }
    // The position counts what was pushed back as not read; a seek drops it.
    if (from == 1) offset -= static_cast<std::int64_t>(r->back.size());
    r->back.clear();
    const int whence = from == 0 ? SEEK_SET : from == 1 ? SEEK_CUR : SEEK_END;
    if (std::fseek(r->f, static_cast<long>(offset), whence) != 0) {
        r->err = errno;
        return -1;
    }
    r->end = false;
    r->reading = false;
    return 0;
}

int Files::atEnd(W4 desc, bool& notOpen) {
    const File* r = reader(desc, notOpen);
    return r != nullptr && r->end ? 1 : 0;
}

int Files::error(W4 desc, std::string& text) {
    bool notOpen = false;
    const File* r = reader(desc, notOpen);
    const int e = r == nullptr ? EBADF : r->err;
    text = e == 0 ? std::string() : std::generic_category().message(e);
    return e;
}

}  // namespace kirimasim::kernel
