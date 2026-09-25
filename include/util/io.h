#pragma once

#include "util/memory.h"
#include "util/synchronized.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef UTIL_ZSTD
#include "fmt/format.h"
#endif

// file IO utilities

namespace util {

// class for reading/writing (binary) files
class File
{
	FILE *file_ = nullptr;

  public:
	File() = default;

	// destructor and move semantics (move-only type)
	~File() noexcept { close(); }
	File(File &&other) noexcept : file_(std::exchange(other.file_, nullptr)) {}
	File &operator=(File &&other) noexcept
	{
		if (this == &other)
			return *this;
		close();
		file_ = std::exchange(other.file_, nullptr);
		return *this;
	}
	File(File const &) = delete;
	File &operator=(File const &) = delete;

	static File open(std::string_view file, bool writeable = false);
	static File create(std::string_view file, bool overwrite = false);
	void close() noexcept;

	explicit operator bool() const { return file_ != nullptr; }

	// get raw FILE* pointer for low-level operations
	FILE *get() const { return file_; }

	// flush internal buffer
	//   * does not guarantee disk write (due to buffering in OS)
	void flush();

	// move position in file
	void seek(size_t pos);
	void skip(size_t bytes);
	size_t tell() const;

	// read/write 'size' bytes from/to file
	void read_raw(void *buffer, size_t size);
	void write_raw(void const *buffer, size_t size);

	// read write a single value of a simple type T
	//     * 'trivially_copyable' ensures the type can be 'copied by memcpy'
	//     * there is still plenty of room for platform-dependence (e.g.,
	//       alignment, endianess) and wrong semantics (e.g. writing a pointer),
	//       so be careful.
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void read(T &value)
	{
		read_raw(&value, sizeof(T));
	}
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void write(T const &value)
	{
		write_raw(&value, sizeof(T));
	}

	// read/write multiple values of a simple type T
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void read(T *data, size_t count)
	{
		read_raw(data, count * sizeof(T));
	}
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void write(T const *data, size_t count)
	{
		write_raw(data, count * sizeof(T));
	}

	// get underlying file descriptor (-1 if file is not open)
	int fd() const;

	// truncate file to the given size
	void truncate(size_t size);
};

// RAII wrapper for a file descriptor. read/write operations are direct
// syscalls, no buffering.
class RawFile
{
	int fd_ = -1;

  public:
	RawFile() = default;
	explicit RawFile(int fd) noexcept;

	~RawFile() noexcept;
	void close() noexcept;

	// move-only
	RawFile(RawFile &&other) noexcept;
	RawFile &operator=(RawFile &&other) noexcept;
	RawFile(RawFile const &) = delete;
	RawFile &operator=(RawFile const &) = delete;

	static RawFile open(std::string_view file, bool writeable = false);
	static RawFile create(std::string_view file, bool overwrite = false);

	explicit operator bool() const noexcept;
	int fd() const noexcept;

	// Read exactly 'size' bytes from the file.
	// - issues multiple read() syscalls if necessary
	// - throws on premature EOF or other errors
	// - might block if the file is a pipe/socket and not enough data is
	//   available
	void read(void *buffer, size_t size);

	// Write exactly 'size' bytes to the file.
	// - issues multiple write() syscalls if necessary
	// - throws on errors
	// - might block if the file is a pipe/socket and not enough buffer space is
	//   available
	void write(void const *buffer, size_t size);
};

#ifdef UTIL_ZSTD
// Append-only file with zstd compression. Writes are buffered and
// compressed/written in chunks.
class ZstdFile : private RawFile
{
	// max (uncompressed) size of a single zstd block is 128 KiB. We aim to call
	// into zstd with at least that much data.
	static constexpr size_t block_size_ = 128 * 1024;

	// max (compressed) size of a single zstd block is 128 KiB + some headers. A
	// smaller write buffer could force zstd to emit shorter blocks, which hurts
	// compression ratio.
	static constexpr size_t write_buffer_size_ = 129 * 1024;

	// starting a new frame worsens overall compression ratio because all
	// context is lost and has to be rebuilt. On the other hand it makes the
	// compressed file nicer to use (e.g. better random access, multithreaded
	// decompression, etc.).
	static constexpr size_t frame_threshold_ = 256 * 1024 * 1024;

	// compression settings. Feel free to change.
	static constexpr int compression_level = 3;
	static constexpr int checksum = 1; // 0 or 1

	void *stream_ = nullptr;

	// buffers
	std::vector<std::byte> buffer_;
	std::vector<std::byte> write_buffer_;

	size_t bytes_processed_ = 0; // bytes sent to zstd
	size_t bytes_written_ = 0;   // bytes written to file
	size_t last_frame_ = 0;      // 'bytes_processed_' at last frame boundary

	// compress and write given data to disk. 'end' should be one of
	// ZSTD_e_continue/flush/end.
	void write_impl(std::span<const std::byte> data, int end);
	void flush_buffer(int end);
	ZstdFile(RawFile &&file, void *stream) noexcept;

  public:
	// number of plain/compressed bytes.
	// - note: these counts stay valid after '.close()' to allow reporting final
	//   statistics. Only reset when a new file is opened.
	// - note: these counts are only exact after a '.flush()' or '.close()'
	//   operation. Otherwise, some data might still be buffered (in this class
	//   or in the zstd library) and not be accounted for yet.
	size_t bytes_processed() const noexcept { return bytes_processed_; }
	size_t bytes_written() const noexcept { return bytes_written_; }

	// constructors
	ZstdFile() = default;
	~ZstdFile() noexcept;
	static ZstdFile create(std::string_view file, bool overwrite = false);

	explicit operator bool() const noexcept { return stream_ != nullptr; }

	// move-only (byte-counts are preserved in moved-from object)
	ZstdFile(ZstdFile &&other) noexcept;
	ZstdFile &operator=(ZstdFile &&other) noexcept;

	void close() noexcept;

	// makes sure all buffered data is written to file
	// - does not guarantee disk write (due to buffering in OS)
	// - finalizes the current "zstd block", which can negatively impact
	//   compression ratio. Effect should be mild though because compression
	//   context/history is preserved.
	// - does not finalize the current "zstd frame". That means a crash after
	//   '.flush()' will not leave a strictly valid zstd file, but typical
	//   decompression tools will happily read all existing data and ignore the
	//   missing frame footer.
	void flush();

	// Write raw data. Buffered.
	void write(void const *data, size_t size);

	// Convenience function for writing formatted text using fmtlib. Buffered.
	template <class... Args>
	void print(fmt::format_string<Args...> format, Args &&...args)
	{
		auto text = fmt::format(format, std::forward<Args>(args)...);
		write(text.data(), text.size());
	}
};
#endif

class MappedFile
{
	void *ptr_ = nullptr;
	size_t size_ = 0;
	MappedFile(char const *, bool);

  public:
	// constructors
	MappedFile() = default;

	static MappedFile open(std::string_view file, bool writeable = false);
	static MappedFile create(std::string_view file, size_t size,
	                         bool overwrite = false);
	void close() noexcept;

	// special members (move-only type)
	~MappedFile() { close(); };
	MappedFile(MappedFile &&other) noexcept
	    : ptr_(std::exchange(other.ptr_, nullptr)),
	      size_(std::exchange(other.size_, 0))
	{}
	MappedFile &operator=(MappedFile &&other) noexcept
	{
		close();
		ptr_ = std::exchange(other.ptr_, nullptr);
		size_ = std::exchange(other.size_, 0);
		return *this;
	}

	// data access
	// NOTE: if the file is opened as read-only, writing should be considered
	//       undefined behaviour, not sure about platform specifics
	void *data() { return ptr_; }
	void const *data() const { return ptr_; }
	size_t size() const { return size_; }
	explicit operator bool() const { return ptr_; }
};

// RAII wrapper for 'eventfd'.
// Only current usecase: implementation detail inside 'InterruptHandler'.
class EventFd
{
	int fd_ = -1;

  public:
	// default constructor creates a null/closed state
	EventFd() = default;

	// create a new eventfd with given initial value (typically 0).
	explicit EventFd(int initial_value);

	~EventFd() noexcept { close(); }

	// close the eventfd. No-op if already closed.
	void close() noexcept;

	// move-only
	EventFd(EventFd const &) = delete;
	EventFd &operator=(EventFd const &) = delete;
	EventFd(EventFd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
	EventFd &operator=(EventFd &&other) noexcept
	{
		if (this == &other)
			return *this;
		close();
		fd_ = std::exchange(other.fd_, -1);
		return *this;
	}

	// get underlying file descriptor. -1 if closed.
	int fd() const noexcept { return fd_; }

	// blocks until value is non-zero, then returns value and resets it to zero
	uint64_t read();

	// Non-blocking variant 'read(), returns 0 if nothing is available.
	uint64_t try_read();

	// increments the value by 'delta'. Non-blocking.
	void write(uint64_t delta = 1);

	// same as 'write', but without exceptions or retry-loop.
	//   - returns true on immediate success, false otherwise
	//   - In normal usage, fails are exceedingly unlikely, so the difference
	//     between 'write' and 'write_safe' is not really detectable.
	//   - This function is guaranteed to be async-signal-safe, so it can be
	//     called from a signal handler. This is the main reason for this class
	//     to exist.
	bool write_safe(uint64_t delta = 1) noexcept;
};

// Create std::stop_token's that are notified when SIGINT is received
// - Re-usable via '.reset()'
// - Only once instance should exist at any time, typically as a global
//   variable.
class InterruptManager
{
	// Implementation:
	// - A signal handler is installed for 'SIGINT' that writes into an
	//   'eventfd' (because that is one of the few things that is guaranteed to
	//   be safe inside a signal handler)
	// - A dedicated thread listens on that 'eventfd' and calls 'request_stop()'
	//   when it gets a notification.
	synchronized<std::stop_source> source_;
	EventFd event_{0};
	std::jthread thread_;

	inline static std::atomic<EventFd *> event_ptr_{nullptr};

	static void signal_handler(int) noexcept;
	void thread_main(std::stop_token stoken);

  public:
	// Default constructor installs the signal handler and starts the thread.
	InterruptManager();

	// destructor uninstalls the signal handler and joins the background thread.
	~InterruptManager() noexcept;

	// get a stop_token that will be notified when SIGINT is received.
	std::stop_token token() const noexcept;

	// reset the internal stop_source, thus starting a new cancellation epoch
	void reset() noexcept;
};

// zstd compression/decompression
std::string decompress(std::span<const std::byte> data);
std::vector<std::byte> compress(std::string_view text,
                                int compression_level = 3);

// convenience functions for reading/writing entire files
//   * reading a text file automatically detects and unpacks zstd compression.
//     Reading binary files does not do this.
std::string read_file(std::string_view filename);
std::vector<std::byte> read_binary_file(std::string_view filename);
void write_file(std::string_view filename, std::string_view data);

} // namespace util
