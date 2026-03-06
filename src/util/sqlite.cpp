#include "util/sqlite.h"

#include "fmt/format.h"
#include <cassert>

#ifdef UTIL_SQLITE3

namespace util {

SqliteError::SqliteError(std::string_view msg)
    : std::runtime_error(std::string(msg))
{}
SqliteError::SqliteError(std::string_view msg, int error_code)
    : SqliteError(fmt::format("{}: {}", msg, sqlite3_errstr(error_code)))
{}
SqliteError::SqliteError(std::string_view msg, sqlite3 *db)
    : SqliteError(fmt::format("{}: {}", msg, sqlite3_errmsg16(db)))
{}

namespace {
void check(int ec, char const *msg)
{
	if (ec != SQLITE_OK)
		throw SqliteError(msg, ec);
}
void check(int ec, char const *msg, sqlite3 *db)
{
	if (ec != SQLITE_OK)
		throw SqliteError(msg, db);
}
} // namespace

void get_value(sqlite3_stmt *stmt, int idx, int &value)
{
	value = sqlite3_column_int(stmt, idx);
}

void get_value(sqlite3_stmt *stmt, int idx, int64_t &value)
{
	value = static_cast<int64_t>(sqlite3_column_int64(stmt, idx));
}

void get_value(sqlite3_stmt *stmt, int idx, float &value)
{
	value = static_cast<float>(sqlite3_column_double(stmt, idx));
}

void get_value(sqlite3_stmt *stmt, int idx, double &value)
{
	value = sqlite3_column_double(stmt, idx);
}

void get_value(sqlite3_stmt *stmt, int idx, std::string &value)
{
	value = reinterpret_cast<char const *>(sqlite3_column_text(stmt, idx));
}

void SqliteStatement::finalize() noexcept
{
	// warning: sqlite3_finalize returns error-code from the last 'step' call,
	// not errors from finalize itself. So we ignore it here.
	sqlite3_finalize(stmt_); // harmless no-op if stmt_==null
	stmt_ = nullptr;
}

SqliteStatement::SqliteStatement(sqlite3 *db, std::string_view sql)
{
	// note: 'sqlite3_prepare' only parses only a single sql statement
	assert(db != nullptr);
	char const *tail = nullptr;
	check(sqlite3_prepare_v2(db, sql.data(), (int)sql.size(), &stmt_, &tail),
	      "could not prepare statement", db);
	if (tail != nullptr && tail != &*sql.end())
		throw SqliteError("unused trailing SQL source");
	assert(stmt_ != nullptr);
}

SqliteStatement &SqliteStatement::operator=(SqliteStatement &&other) noexcept
{
	if (this == &other)
		return *this;
	finalize();
	stmt_ = std::exchange(other.stmt_, nullptr);
	return *this;
}

SqliteStatement::~SqliteStatement() { finalize(); }

SqliteStatement::operator bool() const noexcept { return stmt_ != nullptr; }

int SqliteStatement::column_count() const
{
	return stmt_ ? sqlite3_column_count(stmt_) : 0;
}

int SqliteStatement::parameter_count() const
{
	return stmt_ ? sqlite3_bind_parameter_count(stmt_) : 0;
}

void SqliteStatement::reset()
{
	check(sqlite3_reset(stmt_), "could not reset statement");
}

void SqliteStatement::clear_bindings()
{
	check(sqlite3_clear_bindings(stmt_), "could not clear bindings");
}

void SqliteStatement::bind(int index, std::nullptr_t)
{
	check(sqlite3_bind_null(stmt_, index), "could not bind NULL");
}

void SqliteStatement::bind(int index, int value)
{
	check(sqlite3_bind_int(stmt_, index, value), "could not bind INT");
}

void SqliteStatement::bind(int index, int64_t value)
{
	check(sqlite3_bind_int64(stmt_, index, value), "could not bind INT64");
}

void SqliteStatement::bind(int index, double value)
{
	check(sqlite3_bind_double(stmt_, index, value), "could not bind DOUBLE");
}

void SqliteStatement::bind(int index, std::string_view value)
{
	check(sqlite3_bind_text(stmt_, index, value.data(), (int)value.size(),
	                        SQLITE_TRANSIENT),
	      "could not bind TEXT");
}

bool SqliteStatement::step()
{
	int ec = sqlite3_step(stmt_);
	if (ec == SQLITE_ROW)
		return true;
	if (ec == SQLITE_DONE)
		return false;
	throw SqliteError("sqlite3_step failed", ec);
}

void Sqlite::close() noexcept
{
	// sqlite3_close(null) is harmless no-op
	check(sqlite3_close(db_), "double not close DB");
	db_ = nullptr;
}

Sqlite::Sqlite() = default;

Sqlite::Sqlite(const char *filename)
{
	if (int ec = sqlite3_open(filename, &db_); ec != SQLITE_OK)
	{
		// yes, even on error, calling 'close()' is necessary
		std::string message = db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(ec);
		close();
		throw SqliteError(message);
	}
}

Sqlite::Sqlite(std::string const &filename) : Sqlite(filename.c_str()) {}
Sqlite::Sqlite(std::string_view filename) : Sqlite(std::string(filename)) {}

Sqlite::Sqlite(Sqlite &&other) noexcept : db_(std::exchange(other.db_, nullptr))
{}

Sqlite &Sqlite::operator=(Sqlite &&other) noexcept
{
	if (this == &other)
		return *this;
	close();
	db_ = std::exchange(other.db_, nullptr);
	return *this;
}

Sqlite::~Sqlite() { close(); }

Sqlite::operator bool() const noexcept { return db_ != nullptr; }

SqliteStatement Sqlite::prepare(std::string_view sql) const
{
	if (db_ == nullptr)
		throw SqliteError("no SQLite db connected");
	return SqliteStatement(db_, sql);
}

void Sqlite::execute(std::string_view sql)
{
	auto stmt = prepare(sql);
	while (stmt.step())
	{
	}
}

} // namespace util

#endif
