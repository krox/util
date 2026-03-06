#pragma once

#ifdef UTIL_SQLITE3

#include "sqlite3.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

// Exceptin thrown whenever SQLite gives an error code. Also used for some
// higher-level errors from util::Sqlite (e.g. type-mismatches, that SQLite3
// itself would tolerate)
class SqliteError : public std::runtime_error
{
  public:
	explicit SqliteError(std::string_view msg);
	explicit SqliteError(std::string_view msg, int error_code);
	explicit SqliteError(std::string_view msg, sqlite3 *db);
};

// concept for anything with a 'std::tuple_size'
template <class T>
concept RowType =
    requires { typename std::tuple_size<std::remove_cvref_t<T>>::type; };

// nicely overloaded version of 'sqlite3_column_*(stmt, idx, ...)'
void get_value(sqlite3_stmt *stmt, int column_index, int &value);
void get_value(sqlite3_stmt *stmt, int column_index, int64_t &value);
void get_value(sqlite3_stmt *stmt, int column_index, float &value);
void get_value(sqlite3_stmt *stmt, int column_index, double &value);
void get_value(sqlite3_stmt *stmt, int column_index, std::string &value);

template <RowType Row, std::size_t... I>
void get_row_impl(sqlite3_stmt *stmt, Row &row, std::index_sequence<I...>)
{
	using std::get;
	(get_value(stmt, static_cast<int>(I), get<I>(row)), ...);
}

template <RowType Row> void get_row(sqlite3_stmt *stmt, Row &row)
{
	get_row_impl(stmt, row,
	             std::make_index_sequence<
	                 std::tuple_size_v<std::remove_cvref_t<Row>>>{});
}

// Prepared Statement
// This contains compiled bytecode for a single SQL statement, as well as a
// state machine for executing it. Can be re-used for efficiency.
class SqliteStatement
{
	sqlite3_stmt *stmt_ = nullptr;

  public:
	// compile a single SQL statement to SQLite's internal bytecode
	explicit SqliteStatement(sqlite3 *db, std::string_view sql);

	// move-only type
	SqliteStatement() = default;
	SqliteStatement(const SqliteStatement &) = delete;
	SqliteStatement &operator=(const SqliteStatement &) = delete;
	SqliteStatement(SqliteStatement &&other) noexcept;
	SqliteStatement &operator=(SqliteStatement &&other) noexcept;

	~SqliteStatement();
	void finalize() noexcept;

	// valid statement?
	explicit operator bool() const noexcept;

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

	// run until either
	//   * next output row is produced (returns true)
	//   * statement is finished (returns false)
	//   * error (throws)
	bool step();

	// get a single column value of the current row
	template <class T> T column(int index) const
	{
		T value{};
		get_value(stmt_, index, value);
		return value;
	}

	// get a full row
	template <RowType Row> Row row() const
	{
		Row out;
		get_row(stmt_, out);
		return out;
	}
};

class Sqlite
{
	sqlite3 *db_ = nullptr;

  public:
	Sqlite();
	Sqlite(const Sqlite &) = delete;
	Sqlite &operator=(const Sqlite &) = delete;

	explicit Sqlite(const char *filename);
	explicit Sqlite(std::string const &filename);
	explicit Sqlite(std::string_view filename);

	Sqlite(Sqlite &&other) noexcept;
	Sqlite &operator=(Sqlite &&other) noexcept;

	void close() noexcept;

	~Sqlite();

	explicit operator bool() const noexcept;

	// prepare a statement, does not run it yet
	SqliteStatement prepare(std::string_view sql) const;

	// prepare and run a statement (ignoring any returned rows)
	void execute(std::string_view sql);

	// prepare and run a statement, invoking a callback for each returned row
	template <RowType Row>
	void query(std::string_view sql, std::invocable<Row> auto callback)
	{
		auto stmt = prepare(sql);
		while (stmt.step())
			std::apply(callback, stmt.row<Row>());
	}

	// same, but with individual column types
	template <class... Cols>
	void query(std::string_view sql, std::invocable<Cols...> auto callback)
	{
		auto stmt = prepare(sql);
		while (stmt.step())
		{
			[&]<std::size_t... I>(std::index_sequence<I...>) {
				callback(stmt.template column<Cols>(static_cast<int>(I))...);
			}(std::index_sequence_for<Cols...>{});
		}
	}

	// same, but collect all rows into a single vector
	template <RowType Row> std::vector<Row> query(std::string_view sql)
	{
		std::vector<Row> out;
		auto stmt = prepare(sql);
		while (stmt.step())
			out.emplace_back(stmt.row<Row>());
		return out;
	}

	// same, but only return a single row. Throws if zero or more than one row
	// is returned.
	template <RowType Row> Row query_one(std::string_view sql)
	{
		auto stmt = prepare(sql);
		if (!stmt.step())
			throw SqliteError("query_one: no rows returned");
		Row out = stmt.row<Row>();
		if (stmt.step())
			throw SqliteError("query_one: more than one row returned");
		return out;
	}
};

} // namespace util

#endif
