#pragma once

#ifdef UTIL_SQLITE3

#include "sqlite3.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

//   auto db = util::Sqlite("my_database.sqlite");
//
// Core API with explicit statement handling:
//   auto stmt = db.prepare("SELECT a,b FROM ...");
//   while(stmt.step()) {
//       auto [a, b] = stmt.row<int, std::string>();
//       int a = stmt.column<int>(0);
//       ... use a, b ...
//   }
//
// Short-hand for reading multiple rows
//   db.query<int, std::string>("SELECT a,b FROM ... WHERE id = ?", 42,
//       [&](int a, std::string b)
//       {
//           ... use a, b ...
//       });
//
// Short-hand for a single value
//   std::optional<int> value =
//         db.query_one<int>("SELECT value FROM settings WHERE key = ?", "foo");
//
// Short-hand for one-off, non-query statements
//   db.execute("PRAGMA foreign_keys = ON");
//   db.execute("CREATE TABLE IF NOT EXISTS ..."); // one statement per call

// Exception thrown whenever SQLite gives an error code. Also used for some
// higher-level errors from util::Sqlite (e.g. type-mismatches, that SQLite3
// itself would tolerate)
class SqliteError : public std::runtime_error
{
  public:
	explicit SqliteError(std::string_view msg);
	explicit SqliteError(std::string_view msg, int error_code);
	explicit SqliteError(std::string_view msg, sqlite3 *db);
};

// nicely overloaded version of 'sqlite3_column_*(stmt, idx, ...)'
void get_value(sqlite3_stmt *stmt, int column_index, int &value);
void get_value(sqlite3_stmt *stmt, int column_index, int64_t &value);
void get_value(sqlite3_stmt *stmt, int column_index, float &value);
void get_value(sqlite3_stmt *stmt, int column_index, double &value);
void get_value(sqlite3_stmt *stmt, int column_index, std::string &value);

template <class... Ts, std::size_t... I>
void get_row_impl(sqlite3_stmt *stmt, std::tuple<Ts...> &row,
                  std::index_sequence<I...>)
{
	using std::get;
	(get_value(stmt, static_cast<int>(I), get<I>(row)), ...);
}

template <class... Ts> void get_row(sqlite3_stmt *stmt, std::tuple<Ts...> &row)
{
	get_row_impl(stmt, row, std::index_sequence_for<Ts...>{});
}

// Prepared Statement
// This contains compiled bytecode for a single SQL statement, as well as a
// state machine for executing it. Can be re-used for efficiency.
class SqliteStatement
{
	sqlite3_stmt *stmt_ = nullptr;

  public:
	SqliteStatement() = default;

	// compile a single SQL statement to SQLite's internal bytecode
	explicit SqliteStatement(sqlite3 *db, std::string_view sql);

	// move-only type
	SqliteStatement(const SqliteStatement &) = delete;
	SqliteStatement &operator=(const SqliteStatement &) = delete;
	SqliteStatement(SqliteStatement &&other) noexcept
	    : stmt_(std::exchange(other.stmt_, nullptr))
	{}
	SqliteStatement &operator=(SqliteStatement &&other) noexcept
	{
		if (this != &other)
		{
			finalize();
			stmt_ = std::exchange(other.stmt_, nullptr);
		}
		return *this;
	}

	~SqliteStatement() { finalize(); }
	void finalize() noexcept;

	// valid statement?
	explicit operator bool() const noexcept { return stmt_ != nullptr; }

	// number of parameters
	int parameter_count() const;

	// number of result columns (0 if not a SELECT statement)
	int column_count() const;

	// reset the statement so that it can be run again.
	// NOTE: this does not clear any parameter bindings
	void reset();

	// reset all parameter bindings to 'NULL'
	void clear_bindings();

	// bind a parameter to a value. beware: Indices start at 1 !
	void bind(int index, std::nullptr_t);
	void bind(int index, int value);
	void bind(int index, int64_t value);
	void bind(int index, double value);
	void bind(int index, std::string_view value);

	// bind multiple parameters at once
	template <class... Args> void bind_all(Args &&...args)
	{
		bind_all_impl(1, std::forward<Args>(args)...);
	}

	// run until either
	//   * next output row is produced (returns true)
	//   * statement is finished (returns false)
	//   * error (throws)
	bool step();

	// execute the statement until completion
	void execute()
	{
		while (step())
		{
		}
	}

	// get a single column value of the current row (0-based index)
	template <class T> T column(int index) const
	{
		T value{};
		get_value(stmt_, index, value);
		return value;
	}

	// get a full row
	template <class... Cols> std::tuple<Cols...> row() const
	{
		std::tuple<Cols...> out;
		get_row(stmt_, out);
		return out;
	}

  private:
	template <class T, class... Args>
	void bind_all_impl(int index, T &&value, Args &&...args)
	{
		bind(index, std::forward<T>(value));
		bind_all_impl(index + 1, std::forward<Args>(args)...);
	}
	void bind_all_impl(int) {}
};

class Sqlite
{
	sqlite3 *db_ = nullptr;

  public:
	Sqlite() = default;

	// if 'writeable' is false, any write attempts will fail. If the file does
	// not exist, it will not be created.
	explicit Sqlite(std::string_view filename, bool writeable = true);

	Sqlite(const Sqlite &) = delete;
	Sqlite &operator=(const Sqlite &) = delete;
	Sqlite(Sqlite &&other) noexcept : db_(std::exchange(other.db_, nullptr)) {}
	Sqlite &operator=(Sqlite &&other) noexcept
	{
		if (this != &other)
		{
			close();
			db_ = std::exchange(other.db_, nullptr);
		}
		return *this;
	}

	void close() noexcept;

	~Sqlite() { close(); }

	explicit operator bool() const noexcept { return db_ != nullptr; }

	// prepare a statement, does not run it yet
	SqliteStatement prepare(std::string_view sql) const;

	// ditto, also binding parameters
	template <class... Args>
	    requires(sizeof...(Args) > 0)
	SqliteStatement prepare(std::string_view sql, Args &&...args) const
	{
		auto stmt = prepare(sql);
		stmt.bind_all(std::forward<Args>(args)...);
		return stmt;
	}

	// prepare and run a statement (ignoring any returned rows).
	// Intended for one-off statements like PRAGMA's or schema changes.
	template <class... Args> void execute(std::string_view sql, Args &&...args)
	{
		auto stmt = prepare(sql);
		stmt.bind_all(std::forward<Args>(args)...);
		while (stmt.step())
		{
		}
	}

	// prepare and run a statement, invoking a callback for each returned row.
	// Bind parameters come first and the callback last, as in the example above.
	// A parameter pack is not deduced when it is not the last parameter, so the
	// callback is split off from the argument list here.
	template <class... Cols, class... Ts>
	void query(std::string_view sql, Ts &&...ts)
	{
		constexpr std::size_t n = sizeof...(Ts);
		static_assert(n >= 1, "query() requires a callback");
		auto args = std::forward_as_tuple(std::forward<Ts>(ts)...);
		auto &&callback = std::get<n - 1>(args);
		static_assert(std::invocable<decltype(callback), Cols...>,
		              "query() callback is not invocable with the column types");
		query_impl<Cols...>(sql, callback, args,
		                    std::make_index_sequence<n - 1>{});
	}

	// prepare and run a statement, returning a single row or nullopt if no rows
	// are returned. Any additional rows are ignored.

	template <class... Cols, class... Args>
	    requires(sizeof...(Cols) == 1)
	std::optional<Cols...> query_one(std::string_view sql, Args &&...args)
	{
		auto stmt = prepare(sql, std::forward<Args>(args)...);
		if (!stmt.step())
			return std::nullopt;
		return stmt.template column<Cols...>(0);
	}

	// ditto, for multiple columns
	template <class... Cols, class... Args>
	    requires(sizeof...(Cols) >= 2)
	std::optional<std::tuple<Cols...>> query_one(std::string_view sql,
	                                             Args &&...args)
	{
		auto stmt = prepare(sql, std::forward<Args>(args)...);
		if (!stmt.step())
			return std::nullopt;
		return stmt.template row<Cols...>();
	}

  private:
	template <class... Cols, class F, class Tuple, std::size_t... I>
	void query_impl(std::string_view sql, F &&callback, Tuple &&args,
	                std::index_sequence<I...>)
	{
		auto stmt = prepare(sql, std::get<I>(std::forward<Tuple>(args))...);
		while (stmt.step())
			std::apply(callback, stmt.template row<Cols...>());
	}
};

} // namespace util

#endif
