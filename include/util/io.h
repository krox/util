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
#include <vector>

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

// Effectively converts 'SIGINT' into a 'std::stop_token' notification.
// Implementation details:
//   * A signal handler is installed for 'SIGINT' that writes into an 'eventfd'
//     (because that is one of the few things that is guaranteed to be safe
//     inside a signal handler).
//   * A dedicated thread listens on that 'eventfd' and calls 'request_stop()'
//     on any token given out previously. The internal stop_source is replaced
//     with a new one after each notification, so that we can re-use the
//     'InterruptHandler'
//   * When the 'InterruptHandler' is destroyed, all outstanding tokens are
//     notified as well before the signal handler is uninstalled and the thread
//     is joined. All tokens stay alive independently of the 'InterruptHandler'
//     lifetime, though a typical user will have discarded them before then.
class InterruptManager
{
	synchronized<std::stop_source> source_;
	std::jthread thread_;
	std::atomic<bool> terminate_{false};

	static std::atomic<bool> active_;
	static EventFd event_;
	static void signal_handler(int) noexcept { event_.write_safe(1); }

	void thread_main();

  public:
	// default constructor installs the signal handler and starts the thread.
	// Only one instance of this class should exist at a time, as installing
	// multiple signal handlers is not supported.
	InterruptManager();

	// destructor uninstalls the signal handler and joins the background thread.
	~InterruptManager() noexcept;

	// get a stop_token that will be notified when SIGINT is received.
	//   * Only signals received after the token is obtained will result in
	//     notification. Signals are never queued or made pending.
	//   * The token will also be notified when the 'InterruptManager' is
	//     destroyed. The user cannot distinguish this from an actual SIGINT.
	//     (This semantic is intentional as it helps with some race conditions.
	//     Does not come up in typical use though.)
	//   * The tokens returned by successive calls to 'get_token()' might or
	//     might not refer to the same underlying stop state. Does not matter in
	//     practice.
	std::stop_token token() const noexcept;
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
